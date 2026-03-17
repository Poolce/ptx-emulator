import argparse
import json
from pathlib import Path

from jinja2 import Environment, FileSystemLoader

# Custom methods
regexp_map = {}


class CustomFunctions:
    def make_regexp(instr):
        tmp = instr["regexp_template"]
        for name, arg in instr["params"].items():
            regexp = ""
            if arg["arg_type"] == "qualifier":
                regexp = '|'.join(arg['values'])
            elif arg["arg_type"] == "global":
                regexp = regexp_map["types"][name]["regexp"]
            elif "Operand" in arg["arg_type"]:
                regexp = regexp_map["operands"][arg["arg_type"]]["regexp"]
            else:
                regexp = r".*"
            tmp = tmp.replace(f"${name}", regexp)
        return tmp

    def get_type_name(instr_name, op_name, op):
        arg_type = op["arg_type"]
        if "dtype" in op:
            return op["dtype"]
        if arg_type == "qualifier":
            return f"{instr_name}{op_name}Ql"
        elif "Operand" in arg_type:
            return arg_type
        return ""

    def process_pname(name):
        sname = name.split("::")
        return "".join([i.capitalize() for i in sname])


# Code generation
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
    gen_code(ptx_isa, args.out.resolve())
