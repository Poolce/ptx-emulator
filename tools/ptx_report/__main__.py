"""Entry point: python -m tools.ptx_report [options]

Simplest workflow — let the tool drive everything:

    nvcc kernel.cu -o kernel_bin --cudart=shared -O2 -lineinfo
    python3 -m tools.ptx_report --binary ./kernel_bin -o report.html

The tool will:
  1. Locate cuemu (build/bin/cuemu or PATH) and run it with profiling enabled.
  2. Extract the embedded PTX from the binary via cuobjdump.
  3. Auto-discover source files from .file directives in the PTX.
  4. Aggregate metrics and render a self-contained HTML report.

Bring-your-own profiling dump (legacy / CI):

    python3 -m tools.ptx_report prof.txt --binary ./kernel_bin -o report.html

If --source is omitted, paths embedded in the .file directives of the PTX
are used when the files exist on the current machine.
Compile with -lineinfo to get source-line annotation; without it only the
assembly view and metrics are available.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

from .aggregator import aggregate
from .parser import parse_profiling
from .ptx_parser import merge_ptx, parse_ptx
from .renderer import render_html


# ---------------------------------------------------------------------------
# cuemu helpers
# ---------------------------------------------------------------------------

def _find_cuemu(explicit: Optional[Path]) -> Optional[Path]:
    """Locate the cuemu executable."""
    if explicit is not None:
        return explicit.resolve() if explicit.exists() else None

    # Check PATH first
    found = shutil.which("cuemu")
    if found:
        return Path(found)

    # Walk up from tools/ptx_report/ looking for build/bin/cuemu
    here = Path(__file__).resolve().parent
    for ancestor in [here, here.parent, here.parent.parent]:
        candidate = ancestor / "build" / "bin" / "cuemu"
        if candidate.is_file():
            return candidate

    return None


def _run_emulator(
    cuemu: Path,
    binary: Path,
    prof_output: Path,
    config: Optional[Path] = None,
) -> bool:
    """Run *binary* under cuemu with profiling enabled.

    Emulator output (INFO/ERROR lines) is forwarded to stderr so the user
    can see progress.  Returns True on success.
    """
    cmd = [str(cuemu)]
    if config is not None:
        cmd += ["--config", str(config)]
    cmd += [
        "--collect-profiling",
        "--profiling-output", str(prof_output),
        str(binary),
    ]
    print(f"info: running: {' '.join(cmd)}", file=sys.stderr)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(
            f"error: cuemu exited with code {result.returncode} for {binary}",
            file=sys.stderr,
        )
        return False
    return True


# ---------------------------------------------------------------------------
# cuobjdump helpers
# ---------------------------------------------------------------------------

_CUOBJ_META_RE = re.compile(
    r"^(={4,}"
    r"|arch\s*="
    r"|code version\s*="
    r"|host\s*="
    r"|compile_size\s*="
    r"|compressed"
    r"|identifier\s*="
    r"|ptxasOptions"
    r")\b"
)


def _extract_ptx_via_cuobjdump(binary: Path) -> Optional[str]:
    """Run ``cuobjdump -ptx <binary>`` and return the concatenated PTX text."""
    try:
        result = subprocess.run(
            ["cuobjdump", "-ptx", str(binary)],
            capture_output=True,
            text=True,
            timeout=60,
        )
    except FileNotFoundError:
        return None
    except subprocess.TimeoutExpired:
        return None

    if result.returncode != 0:
        return None

    ptx_blocks: list[str] = []
    current_lines: list[str] = []
    in_ptx_section = False

    for raw in result.stdout.splitlines(keepends=True):
        stripped = raw.strip()

        if "Fatbin ptx code:" in stripped:
            if current_lines:
                ptx_blocks.append("".join(current_lines))
            current_lines = []
            in_ptx_section = True
            continue

        if stripped.startswith("Fatbin") and in_ptx_section:
            ptx_blocks.append("".join(current_lines))
            current_lines = []
            in_ptx_section = False
            continue

        if in_ptx_section and not _CUOBJ_META_RE.match(stripped):
            current_lines.append(raw)

    if current_lines:
        ptx_blocks.append("".join(current_lines))

    return "\n".join(ptx_blocks) if ptx_blocks else None


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="python -m tools.ptx_report",
        description="Generate an HTML profiling report from cuemu profiling output.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument(
        "profiling",
        metavar="PROFILING_FILE",
        nargs="?",
        type=Path,
        default=None,
        help=(
            "Pre-existing profiling dump produced by "
            "cuemu --collect-profiling. "
            "If omitted, the emulator is invoked automatically for each --binary."
        ),
    )
    p.add_argument(
        "--binary",
        metavar="FILE",
        type=Path,
        action="append",
        default=[],
        dest="binaries",
        help=(
            "CUDA binary to profile and/or extract PTX from. "
            "Repeat for multiple binaries. "
            "When PROFILING_FILE is not given, cuemu is invoked automatically. "
            "Compile with -lineinfo for source-line annotation."
        ),
    )
    p.add_argument(
        "--ptx",
        metavar="FILE",
        type=Path,
        action="append",
        default=[],
        dest="ptx_files",
        help=(
            "PTX file with optional .loc lineinfo (nvcc --ptx -lineinfo). "
            "Repeat for multiple. Used only when --binary is not given."
        ),
    )
    p.add_argument(
        "--source",
        metavar="FILE",
        type=Path,
        action="append",
        default=[],
        dest="source_files",
        help=(
            "CUDA source file (.cu). Repeat for multiple. "
            "If omitted, paths from .file directives in the PTX are tried."
        ),
    )
    p.add_argument(
        "--cuemu",
        metavar="PATH",
        type=Path,
        default=None,
        help="Path to the cuemu executable (default: auto-detected).",
    )
    p.add_argument(
        "--config",
        metavar="FILE",
        type=Path,
        default=None,
        help=(
            "GPU architecture config file (TOML). "
            "If omitted, built-in defaults are used (Ampere sm_80 profile)."
        ),
    )
    p.add_argument(
        "-o", "--output",
        metavar="FILE",
        type=Path,
        default=Path("report.html"),
        help="Output HTML file (default: report.html)",
    )
    p.add_argument(
        "--title",
        default="PTX Emulator Profile Report",
        help='Report title (default: "PTX Emulator Profile Report")',
    )
    return p


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)

    # ---- validate top-level inputs -----------------------------------------
    if args.profiling is None and not args.binaries:
        print(
            "error: provide PROFILING_FILE or --binary <executable>",
            file=sys.stderr,
        )
        _build_parser().print_usage(sys.stderr)
        return 1

    # ---- profiling output --------------------------------------------------
    profiling_parts: list[str] = []

    if args.profiling is not None:
        # Use the supplied dump directly
        try:
            profiling_parts.append(args.profiling.read_text())
        except OSError as e:
            print(f"error: cannot read {args.profiling}: {e}", file=sys.stderr)
            return 1

    if not args.profiling and args.binaries:
        # Auto-run: no dump supplied — invoke cuemu for each binary
        cuemu = _find_cuemu(args.cuemu)
        if cuemu is None:
            print(
                "error: cuemu not found on PATH or in build/bin/. "
                "Build the project first or pass --cuemu <path>.",
                file=sys.stderr,
            )
            return 1

        for binary in args.binaries:
            if not binary.exists():
                print(f"error: binary not found: {binary}", file=sys.stderr)
                return 1

            with tempfile.NamedTemporaryFile(
                suffix=".prof", prefix="cuemu_", delete=False
            ) as tmp:
                tmp_path = Path(tmp.name)

            try:
                ok = _run_emulator(cuemu, binary, tmp_path, config=args.config)
                if not ok:
                    return 1
                profiling_parts.append(tmp_path.read_text())
            finally:
                tmp_path.unlink(missing_ok=True)

    combined_profiling = "\n".join(profiling_parts)
    records = parse_profiling(combined_profiling)
    if not records:
        print("warning: no profiling records found", file=sys.stderr)

    # ---- PTX extraction ----------------------------------------------------
    ptx_texts: list[str] = []

    if args.binaries:
        for binary in args.binaries:
            if not binary.exists():
                print(f"warning: binary not found: {binary}", file=sys.stderr)
                continue
            text = _extract_ptx_via_cuobjdump(binary)
            if text is None:
                print(
                    f"warning: cuobjdump failed for {binary} "
                    "(is cuobjdump on PATH?)",
                    file=sys.stderr,
                )
            elif not text.strip():
                print(
                    f"warning: no PTX sections in {binary} "
                    "(binary may not embed PTX)",
                    file=sys.stderr,
                )
            else:
                has_loc = ".loc" in text
                print(
                    f"info: extracted PTX from {binary.name}"
                    + (" (with .loc — source mapping available)" if has_loc
                       else " (no .loc — compile with -lineinfo for source mapping)"),
                    file=sys.stderr,
                )
                ptx_texts.append(text)

        if args.ptx_files:
            print("info: --ptx ignored because --binary was specified", file=sys.stderr)

    else:
        # No --binary: use explicit PTX files
        for ptx_path in args.ptx_files:
            try:
                ptx_texts.append(ptx_path.read_text())
            except OSError as e:
                print(f"warning: cannot read {ptx_path}: {e}", file=sys.stderr)

    if not ptx_texts and args.source_files:
        print(
            "info: --source has no effect without PTX info "
            "(pass --binary or --ptx)",
            file=sys.stderr,
        )

    # ---- build PTX module --------------------------------------------------
    ptx = None
    if ptx_texts:
        ptx = merge_ptx([parse_ptx(t) for t in ptx_texts])
        ptx.load_sources(args.source_files)

    # ---- aggregate + render ------------------------------------------------
    report = aggregate(records, ptx)
    report.title = args.title

    if not report.functions:
        print("error: no functions found after aggregation", file=sys.stderr)
        return 1

    html_out = render_html(report)
    try:
        args.output.write_text(html_out)
    except OSError as e:
        print(f"error: cannot write {args.output}: {e}", file=sys.stderr)
        return 1

    n = sum(len(fn.instructions) for fn in report.functions)
    print(
        f"Report written to {args.output} "
        f"({len(report.functions)} kernel(s), {n} unique PCs)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
