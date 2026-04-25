import argparse
import json
import re
from pathlib import Path

from jinja2 import Environment, FileSystemLoader

regexp_map = {}


class CustomFunctions:
    def make_regexp(instr):
        tmp = instr["regexp_template"]
        for name, arg in instr["params"].items():
            regexp = ""
            if "regexp" in arg:
                regexp = arg["regexp"]
            elif arg["arg_type"] == "qualifier":
                regexp = '|'.join(arg['values'])
            elif arg["arg_type"] == "children":
                regexp = '|'.join(arg['values'])
            elif arg["arg_type"] == "operand":
                regexp = regexp_map["operands"][arg["dtype"]]["regexp"]
            elif "dtype" in arg:
                regexp = regexp_map["types"][arg["dtype"]]["regexp"]
            else:
                regexp = r".*"
            tmp = tmp.replace(f"${name}", regexp)
        return tmp

    def get_type_name(instr_name, op_name, op):
        if "dtype" in op:
            return op["dtype"]
        if op["arg_type"] == "qualifier":
            return f"{instr_name}{op_name}Ql"
        if op["arg_type"] == "children":
            iname = instr_name.capitalize()
            parts = op_name.lower().split('_')
            oname = ''.join(word.capitalize() for word in parts)
            return f"local{iname}{oname}Type"
        return ""

    def process_pname(name):
        delimiters = r"[ \.;:]+"
        sname = re.split(delimiters, name)
        return "".join([i.capitalize() for i in sname])


_SCRIPT_DIR = Path(__file__).resolve().parent
env = Environment(loader=FileSystemLoader(str(_SCRIPT_DIR / "templates")),
                  trim_blocks=True,
                  lstrip_blocks=True)


def gen_instruction_header(isa):
    header_template = env.get_template("instruction.h.j2")
    return header_template.render(instructions=isa["instructions"],
                                  funcs=CustomFunctions)


def gen_instruction_source(isa):
    header_template = env.get_template("instruction.cpp.j2")
    return header_template.render(instructions=isa["instructions"],
                                  funcs=CustomFunctions)


def gen_types_header(isa):
    header_template = env.get_template("ptx_types.h.j2")
    return header_template.render(isa=isa,
                                  funcs=CustomFunctions)


def gen_types_source(isa):
    header_template = env.get_template("ptx_types.cpp.j2")
    return header_template.render(isa=isa,
                                  funcs=CustomFunctions)


def gen_code(isa, out_dir: Path):
    env.filters['capitalize'] = lambda s: s.capitalize()
    env.filters['to_camel'] = lambda s: ''.join(w.capitalize()
                                                for w in s.split('_'))
    out_dir.mkdir(parents=True, exist_ok=True)

    content = gen_instruction_header(isa)
    with open(out_dir / "instructions.h", "w") as f:
        f.write(content)

    content = gen_instruction_source(isa)
    with open(out_dir / "instructions.cpp", "w") as f:
        f.write(content)

    content = gen_types_header(isa)
    with open(out_dir / "ptx_types.h", "w") as f:
        f.write(content)

    content = gen_types_source(isa)
    with open(out_dir / "ptx_types.cpp", "w") as f:
        f.write(content)


def load_profiling_metrics(isa):
    profiling_path = _SCRIPT_DIR / "profiling_metrics.json"
    profiling = {"metrics": {}, "instructions": {}}
    if profiling_path.exists():
        with open(profiling_path, "r") as f:
            profiling = json.load(f)

    default_metrics = profiling["instructions"].get("*", {}).get("metrics", [])
    for i_name, instr in isa["instructions"].items():
        if "regexp_template" not in instr:
            instr["profiling_metrics"] = []
            continue
        instr_specific = profiling["instructions"].get(i_name, {})\
            .get("metrics", [])
        merged = list(default_metrics)
        for m in instr_specific:
            if m not in merged:
                merged.append(m)
        instr["profiling_metrics"] = [
            {"name": m, "type": profiling["metrics"][m]["type"]}
            for m in merged
            if m in profiling["metrics"]
        ]


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        type=Path,
        default=_SCRIPT_DIR / "autogen",
        help="Output directory for generated sources/headers",
    )
    args = parser.parse_args()

    with open(_SCRIPT_DIR / "instructions.json", "r") as file:
        ptx_isa = json.load(file)

    regexp_map["operands"] = ptx_isa["operands"]
    regexp_map["types"] = ptx_isa["types"]
    load_profiling_metrics(ptx_isa)
    gen_code(ptx_isa, args.out.resolve())
