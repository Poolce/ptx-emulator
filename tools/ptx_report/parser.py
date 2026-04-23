"""Parse cuemu profiling output text into structured records."""

from __future__ import annotations

import re
from dataclasses import dataclass, field


# [19:47:48.689] [PC: 0x0000000000000004]  Block: [0, 0, 0] WarpId: 12 Execution Mask 0x0000ffff ld branch_efficiency: 1.0000, bank_conflicts: 0
_RECORD_RE = re.compile(
    r"\[(\d{2}:\d{2}:\d{2}\.\d{3})\] "
    r"\[PC: 0x([0-9a-fA-F]+)\]  "
    r"Block: \[(\d+), (\d+), (\d+)\] "
    r"WarpId: (\d+) "
    r"Execution Mask 0x([0-9a-fA-F]+) "
    r"(\w+)"
    r"(.*)"
)

_METRIC_RE = re.compile(r"(\w+):\s*([^\s,]+)")


@dataclass
class ProfilingRecord:
    timestamp: str
    pc: int
    block: tuple[int, int, int]
    warp_id: int
    execution_mask: int
    function_name: str
    basic_block: str
    instr_name: str
    metrics: dict[str, str] = field(default_factory=dict)

    @property
    def active_threads(self) -> int:
        return bin(self.execution_mask).count("1")


def parse_profiling(text: str) -> list[ProfilingRecord]:
    """Parse full profiling output into a flat list of records."""
    records: list[ProfilingRecord] = []
    cur_function = ""
    cur_bb = ""

    for raw in text.splitlines():
        line = raw.rstrip()
        if line.startswith("Function "):
            cur_function = line[len("Function "):]
            cur_bb = ""
            continue
        if line.startswith("Basic Block "):
            cur_bb = line[len("Basic Block "):]
            continue

        m = _RECORD_RE.match(line)
        if not m:
            continue

        ts, pc_hex, bx, by, bz, wid, mask_hex, instr_name, metrics_str = m.groups()
        metrics = {k: v for k, v in _METRIC_RE.findall(metrics_str)}

        records.append(ProfilingRecord(
            timestamp=ts,
            pc=int(pc_hex, 16),
            block=(int(bx), int(by), int(bz)),
            warp_id=int(wid),
            execution_mask=int(mask_hex, 16),
            function_name=cur_function,
            basic_block=cur_bb,
            instr_name=instr_name,
            metrics=metrics,
        ))

    return records
