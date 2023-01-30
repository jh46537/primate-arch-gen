#!/home/amans/anaconda3/bin/python3
import re
import argparse

class VLIW_Instr:
    def __init__(self, instr: str) -> None:
        self.instrs = instr.split("\n")
        self.instrs_toks = [[y.strip() for y in re.split("[,=()]", x)] for x in self.instrs]
        self.isntr_raw_str = instr
        self.live_in_vars = []
        self.live_out_vars = []

        for tok_list in self.instrs_toks:
            if tok_list[0].startswith("%"):
                # found a register!
                self.live_out_vars.append(tok_list[0])
        
        for tok_list in self.instrs_toks:
            for tok in tok_list[1:]:
                var_name = tok.split(" ")[-1]
                if var_name.startswith("%"):
                    self.live_in_vars.append(var_name)

    # check if the live-ins match the live-outs from a block 
    def is_dep_instr(self, older_instr: "VLIW_Instr") -> bool:
        for live_in in self.live_in_vars:
            if live_in in older_instr.live_out_vars:
                return True
        return False

    def __str__(self):
        return str(self.instrs) + "\n"

def generate_instructions(input_file):
    instructions = []

    running_instr_string = ""
    in_code_region = False
    for line in input_file:
        if line.startswith("VLIW Inst"):
            if len(running_instr_string) > 0:
                instructions.append(VLIW_Instr(running_instr_string))
                running_instr_string = ""
            else:
                in_code_region = True
        elif line.startswith("numALU:") or line.startswith("Number of "):
            continue
        elif in_code_region:
            running_instr_string += line.strip()+"\n"

    return instructions

def VLIW_dep_check_pass(instructions: list[VLIW_Instr]):
    window_size = 50

    num_fixed = 0
    for index, instr in enumerate(instructions):
        first_instr_idx = max(index - window_size, 0)
        opt_window = instructions[first_instr_idx:index]
        can_fit_into_window = True
        for prev_instr in opt_window:
            if instr.is_dep_instr(prev_instr):
                can_fit_into_window = False
                break
        # if can_fit_into_window:
        #     for i in opt_window:
        #         if len(i.instrs) >= 4:
        #             can_fit_into_window = False
        
        if can_fit_into_window:
            num_fixed += 1
            print("Instruction", index, "can fit into the instructions before it")

def main(args):
    print("Reading from file", args.input_path+"....")
    file_name = args.input_path
    inp_file = open(file_name, "r")

    instructions = generate_instructions(inp_file)

    print("="*10, "live in for program", "="*10)
    print("\n".join([str(x.live_in_vars) for x in instructions]))

    print("="*10, "live out for program", "="*10)
    print("\n".join([str(x.live_out_vars) for x in instructions]))

    VLIW_dep_check_pass(instructions)

    print("instructions processed:", len(instructions))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Post-compiler pass.')
    parser.add_argument('--input-file', dest='input_path', action='store',
                        help='input file containing the VLIW instructions from primate')

    args = parser.parse_args()
    main(args)

