"""Aggregate raw profiling records into per-PC and per-source-line statistics."""

from __future__ import annotations

import subprocess
from dataclasses import dataclass, field
from typing import Optional

from .parser import ProfilingRecord
from .ptx_parser import PtxModule


@dataclass
class InstrStats:
    pc: int
    function_name: str
    basic_block: str
    instr_name: str
    exec_count: int = 0
    branch_efficiency_sum: float = 0.0
    bank_conflicts_total: int = 0
    # Filled in from PTX module (if provided)
    ptx_line: str = ""
    source_file: Optional[str] = None
    source_line: Optional[int] = None
    source_col: Optional[int] = None

    @property
    def avg_branch_efficiency(self) -> float:
        return self.branch_efficiency_sum / self.exec_count if self.exec_count > 0 else 1.0

    @property
    def avg_bank_conflicts(self) -> float:
        return self.bank_conflicts_total / self.exec_count if self.exec_count > 0 else 0.0


@dataclass
class SourceLineStats:
    line_num: int
    source_text: str
    exec_count: int = 0
    branch_efficiency_sum: float = 0.0
    bank_conflicts_total: int = 0
    pc_count: int = 0

    @property
    def avg_branch_efficiency(self) -> float:
        return self.branch_efficiency_sum / self.pc_count if self.pc_count > 0 else 1.0


@dataclass
class FunctionReport:
    name: str
    demangled_name: str
    instructions: list[InstrStats]  # sorted by PC
    source_lines: dict[str, dict[int, SourceLineStats]]  # file -> {line -> stats}
    total_exec_count: int = 0
    branch_efficiency_sum: float = 0.0
    total_bank_conflicts: int = 0

    @property
    def avg_branch_efficiency(self) -> float:
        count = sum(i.exec_count for i in self.instructions)
        return self.branch_efficiency_sum / count if count > 0 else 1.0

    @property
    def has_source(self) -> bool:
        return any(i.source_line is not None for i in self.instructions)

    @property
    def has_bank_conflicts(self) -> bool:
        return self.total_bank_conflicts > 0

    def hotspots_by_exec(self, n: int = 10) -> list[InstrStats]:
        return sorted(self.instructions, key=lambda i: i.exec_count, reverse=True)[:n]

    def hotspots_by_divergence(self, n: int = 10) -> list[InstrStats]:
        divergent = [i for i in self.instructions if i.exec_count > 0 and i.avg_branch_efficiency < 1.0]
        return sorted(divergent, key=lambda i: i.avg_branch_efficiency)[:n]

    def hotspots_by_conflicts(self, n: int = 10) -> list[InstrStats]:
        conflicted = [i for i in self.instructions if i.bank_conflicts_total > 0]
        return sorted(conflicted, key=lambda i: i.bank_conflicts_total, reverse=True)[:n]


@dataclass
class Report:
    functions: list[FunctionReport]
    title: str = "PTX Emulator Profile Report"
    # source_file_path -> lines list (index 0 unused, 1-based)
    source_files: dict[str, list[str]] = field(default_factory=dict)


def _demangle(name: str) -> str:
    try:
        r = subprocess.run(
            ["c++filt", name],
            capture_output=True, text=True, timeout=5,
        )
        if r.returncode == 0:
            result = r.stdout.strip()
            if result and result != name:
                return result
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass
    return name


def aggregate(
    records: list[ProfilingRecord],
    ptx: Optional[PtxModule] = None,
) -> Report:
    """Build a Report from flat profiling records, optionally enriched with PTX info."""

    # Group records by function
    func_pcs: dict[str, dict[int, InstrStats]] = {}

    for rec in records:
        fn = rec.function_name
        if fn not in func_pcs:
            func_pcs[fn] = {}

        pc = rec.pc
        if pc not in func_pcs[fn]:
            func_pcs[fn][pc] = InstrStats(
                pc=pc,
                function_name=fn,
                basic_block=rec.basic_block,
                instr_name=rec.instr_name,
            )

        stats = func_pcs[fn][pc]
        stats.exec_count += 1

        be = rec.metrics.get("branch_efficiency")
        if be is not None:
            try:
                stats.branch_efficiency_sum += float(be)
            except ValueError:
                pass

        bc = rec.metrics.get("bank_conflicts")
        if bc is not None:
            try:
                stats.bank_conflicts_total += int(bc)
            except ValueError:
                pass

    # Enrich with PTX info
    if ptx:
        for fn, pc_map in func_pcs.items():
            for pc, stats in pc_map.items():
                ptx_instr = ptx.by_func_pc.get((fn, pc))
                if ptx_instr:
                    stats.ptx_line = ptx_instr.raw_line
                    stats.source_file = ptx_instr.source_file
                    stats.source_line = ptx_instr.source_line
                    stats.source_col = ptx_instr.source_col

    # Build per-source-line aggregates
    function_reports: list[FunctionReport] = []

    for fn, pc_map in func_pcs.items():
        instructions = sorted(pc_map.values(), key=lambda i: i.pc)

        source_lines: dict[str, dict[int, SourceLineStats]] = {}

        for instr in instructions:
            if instr.source_file and instr.source_line is not None:
                sf = instr.source_file
                ln = instr.source_line
                if sf not in source_lines:
                    source_lines[sf] = {}
                if ln not in source_lines[sf]:
                    src_text = ""
                    if ptx:
                        src_text = ptx.get_source_line(sf, ln) or ""
                    source_lines[sf][ln] = SourceLineStats(
                        line_num=ln,
                        source_text=src_text,
                    )
                sl = source_lines[sf][ln]
                sl.exec_count += instr.exec_count
                sl.branch_efficiency_sum += instr.branch_efficiency_sum
                sl.bank_conflicts_total += instr.bank_conflicts_total
                sl.pc_count += instr.exec_count  # weight by exec_count for avg

        total_exec = sum(i.exec_count for i in instructions)
        be_sum = sum(i.branch_efficiency_sum for i in instructions)
        bc_total = sum(i.bank_conflicts_total for i in instructions)

        function_reports.append(FunctionReport(
            name=fn,
            demangled_name=_demangle(fn),
            instructions=instructions,
            source_lines=source_lines,
            total_exec_count=total_exec,
            branch_efficiency_sum=be_sum,
            total_bank_conflicts=bc_total,
        ))

    # Sort functions by total exec count descending
    function_reports.sort(key=lambda f: f.total_exec_count, reverse=True)
    source_files = dict(ptx.source_files) if ptx else {}
    return Report(functions=function_reports, source_files=source_files)
