import argparse
from pathlib import Path
import os
from elftools.elf.elffile import ELFFile

parser = argparse.ArgumentParser(
                    prog='primate-elf-loader',
                    description='converts an ELF exec into a set of files for simulation (ROMs)',
                    epilog='Shout at Kayvan if this fails')
parser.add_argument('-i', '--input_file', type=str, help='Path to ELF executable', required=True)
parser.add_argument('-o', '--output_dir', type=str, help='Output directory for HEX files', default='./primate-a-out/')
parser.add_argument('-p', '--primate_cfg', type=str, help='Path to primate.cfg', required=True)
parser.add_argument('--verbose', action='store_true', help='Prints debug information', default=False)

args = parser.parse_args()

def parse_primate_cfg() -> dict:
    with open(args.primate_cfg) as f:
        ret = {x.split("=")[0] : x.split("=")[1].strip() for x in f}
    ext_and_ins_slots = int(ret['NUM_ALUS']) * 3
    fu_slots          = max(int(ret['NUM_ALUS']), int(ret['NUM_BFUS'])+2)
    ret['VLIW_SLOT_COUNT'] = fu_slots + ext_and_ins_slots + 1
    return ret
            

def print_all_sections(elf_fpath: str):
    f = open(elf_fpath, 'rb')
    elffile = ELFFile(f)

    for section in elffile.iter_sections():
        print(section.name)
    f.close()

def get_text_hex(elf_fpath: str) -> str:
    f = open(elf_fpath, 'rb')
    vliw_slot_count = parse_primate_cfg()['VLIW_SLOT_COUNT']
    elffile = ELFFile(f)
    for section in elffile.iter_sections():
        if section.name == ".text":
            if args.verbose:
                print("text found!")
            text_section = section
            
    if text_section.compressed and args.verbose:
        print("can't handle compressed elfs!")

    text_bytes = text_section.data()
    int_text = [int.from_bytes(text_bytes[n:(n+4)], 'little') for n in range(0, len(text_bytes), 4)]
    current_vliw_instr = []
    program = ""
    for i, x in enumerate(int_text):
        if i % vliw_slot_count == 0 and i != 0:
            if args.verbose:
                print(''.join(current_vliw_instr[::-1]))
            program += ''.join(current_vliw_instr[::-1]) + "\n"
            current_vliw_instr = []
        current_vliw_instr.append(f"{x:0{8}x}")
            
    if args.verbose:
        print(''.join(current_vliw_instr[::-1]))
    program += ''.join(current_vliw_instr[::-1]) + "\n"
    
    f.close()
    
    return program

def get_memory_hex(elf_fpath: str) -> str:
    f = open(elf_fpath, 'rb')
    elffile = ELFFile(f)
    rodata_section  = None
    data_section    = None
    memory_contents = []
    for section in elffile.iter_sections():
        if section.name == ".rodata":
            if args.verbose:
                print("rodata found!")
                print("offset:", section['sh_offset'], "size:", section.data_size)
            rodata_section = section
        if section.name == ".data":
            if args.verbose:
                print("data found!")
                print("offset:", section['sh_offset'], "size:", section.data_size)
            data_section = section

    if rodata_section is not None:
        if rodata_section['sh_offset'] % 4 != 0:
            print("rodata offset is not multiple of 4!")
            print("We can't cope with that")
            exit(-1)
        offset_in_words = int(rodata_section['sh_offset'] / 4)
        for i in range(offset_in_words):
            if args.verbose:
                print("00000000")
            memory_contents.append("00000000")
            
        rodata_bytes = rodata_section.data()
        int_rodata = [int.from_bytes(rodata_bytes[n:(n+4)], 'little') for n in range(0, len(rodata_bytes), 4)]
        
        if args.verbose:
            print(int_rodata)
        for i in int_rodata:
            memory_contents.append(f"{i:0{8}x}")
            
    if data_section is not None:
        pass
    
    f.close()

    return '\n'.join(memory_contents)

if args.verbose:
    print(parse_primate_cfg())
    
in_fpath = args.input_file
if args.verbose:
    print_all_sections(in_fpath)
    
Path(args.output_dir).mkdir(parents=True, exist_ok=True)

text_file = open(os.path.join(args.output_dir, "primate_pgm_text"), "w")
program_text = get_text_hex(in_fpath)
print(program_text, file=text_file)
text_file.close()

text_file = open(os.path.join(args.output_dir, "primate_pgm_mem"), "w")
program_text = get_memory_hex(in_fpath)
print(program_text, file=text_file)
text_file.close()

if args.verbose:
    print("output", args.output_dir)
