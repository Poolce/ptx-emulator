from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

_FUNC_RE = re.compile(
    r"(.*?)\s*\.(entry|func)\s+([A-Za-z0-9_]+)\s*\([^)]*\)\s*\{([^}]+)\}",
    re.DOTALL,
)
_LABEL_RE = re.compile(r"^\s*\$([A-Za-z0-9_]+):\s*$")
_INSTR_RE = re.compile(r"^\s*\.?(?:@%p\d+\s+)?([a-z][a-z0-9]*2?)\b")
_LOC_RE = re.compile(r"^\s*\.loc\s+(\d+)\s+(\d+)\s+(\d+)")
_FILE_RE = re.compile(r'^\s*\.file\s+(\d+)\s+"(.+?)"', re.MULTILINE)
_REG_DECL_RE = re.compile(
    r"^\s*\.reg\s+\.(\w+)\s+%([a-z]+)<(\d+)>"
)
_MEM_REF_RE = re.compile(r"\[[^\]]*\]")
_DST_REG_RE = re.compile(r"%([a-z]+)\d")

_NO_DST_REG = frozenset(
    {"st", "bar", "bra", "ret", "exit", "trap", "loc", "pragma",
     "reg", "shared", "atom", "red"}
)


@dataclass
class PtxInstr:
    pc: int
    function_name: str
    basic_block: str
    instr_name: str
    raw_line: str
    source_file: Optional[str] = None
    source_line: Optional[int] = None
    source_col: Optional[int] = None
    dst_reg_prefix: Optional[str] = None


@dataclass
class PtxModule:
    instructions: list[PtxInstr] = field(default_factory=list)
    by_func_pc: dict[tuple[str, int], PtxInstr] = field(
        default_factory=dict
    )
    source_files: dict[str, list[str]] = field(default_factory=dict)
    func_reg_declarations: dict[str, dict[str, tuple[int, str]]] = field(
        default_factory=dict
    )

    def load_sources(self, provided: list[Path]) -> None:
        referenced = {
            instr.source_file
            for instr in self.instructions
            if instr.source_file
        }
        candidates = {p.resolve() for p in provided}

        for path_str in referenced:
            path = Path(path_str)
            candidates_by_name = {
                p for p in candidates if p.name == path.name
            }
            resolved = None
            if path.exists():
                resolved = path
            elif candidates_by_name:
                resolved = next(iter(candidates_by_name))

            if resolved and resolved.exists():
                try:
                    lines = [""] + resolved.read_text(
                        errors="replace"
                    ).splitlines()
                    self.source_files[path_str] = lines
                except OSError:
                    pass

        for p in candidates:
            key = str(p)
            if key not in self.source_files and p.exists():
                try:
                    lines = [""] + p.read_text(
                        errors="replace"
                    ).splitlines()
                    self.source_files[key] = lines
                    for instr in self.instructions:
                        if (
                            instr.source_file
                            and Path(instr.source_file).name == p.name
                        ):
                            instr.source_file = key
                except OSError:
                    pass

    def get_source_line(
        self, source_file: Optional[str], line: Optional[int]
    ) -> Optional[str]:
        if not source_file or not line:
            return None
        lines = self.source_files.get(source_file)
        if lines and 1 <= line < len(lines):
            return lines[line]
        return None


def _parse_basic_blocks(
    content: str, func_start_pc: int
) -> list[tuple[str, str, list[str]]]:
    blocks: list[tuple[str, list[str]]] = []
    current_name = "entry"
    current_lines: list[str] = []

    for raw in content.splitlines():
        lm = _LABEL_RE.match(raw)
        if lm:
            blocks.append((current_name, current_lines))
            current_name = lm.group(1)
            current_lines = []
        else:
            current_lines.append(raw)

    blocks.append((current_name, current_lines))
    return blocks


def merge_ptx(modules: list[PtxModule]) -> PtxModule:
    merged = PtxModule()
    for mod in modules:
        merged.instructions.extend(mod.instructions)
        merged.by_func_pc.update(mod.by_func_pc)
        merged.source_files.update(mod.source_files)
        merged.func_reg_declarations.update(mod.func_reg_declarations)
    return merged


def parse_ptx(ptx_text: str) -> PtxModule:
    module = PtxModule()

    files: dict[int, str] = {}
    for m in _FILE_RE.finditer(ptx_text):
        files[int(m.group(1))] = m.group(2)

    pc = 0
    for func_m in _FUNC_RE.finditer(ptx_text):
        func_name = func_m.group(3)
        content = func_m.group(4)

        blocks = _parse_basic_blocks(content, pc)

        cur_file_id: Optional[int] = None
        cur_line: Optional[int] = None
        cur_col: Optional[int] = None
        func_regs: dict[str, tuple[int, str]] = {}

        for bb_name, bb_lines in blocks:
            for raw in bb_lines:
                loc_m = _LOC_RE.match(raw)
                if loc_m:
                    cur_file_id = int(loc_m.group(1))
                    cur_line = int(loc_m.group(2))
                    cur_col = int(loc_m.group(3))
                    continue

                instr_m = _INSTR_RE.match(raw)
                if not instr_m:
                    continue

                name = instr_m.group(1)
                if name == "loc":
                    continue

                if name == "reg":
                    reg_decl_m = _REG_DECL_RE.match(raw)
                    if reg_decl_m:
                        type_name = reg_decl_m.group(1)
                        prefix = reg_decl_m.group(2)
                        count = int(reg_decl_m.group(3))
                        func_regs[prefix] = (count, type_name)

                src_file = (
                    files.get(cur_file_id)
                    if cur_file_id is not None
                    else None
                )
                instr = PtxInstr(
                    pc=pc,
                    function_name=func_name,
                    basic_block=bb_name,
                    instr_name=name,
                    raw_line=raw.strip(),
                    source_file=src_file,
                    source_line=cur_line,
                    source_col=cur_col,
                )

                if name not in _NO_DST_REG:
                    rest = raw[instr_m.end():]
                    no_mem = _MEM_REF_RE.sub("", rest)
                    dst_m = _DST_REG_RE.search(no_mem)
                    if dst_m:
                        instr.dst_reg_prefix = dst_m.group(1)

                module.instructions.append(instr)
                module.by_func_pc[(func_name, pc)] = instr
                pc += 1

        if func_regs:
            module.func_reg_declarations[func_name] = func_regs

    return module
