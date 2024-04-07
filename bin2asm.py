#! /bin/python3
import re
import sys

# pkt size in bytes
PACKET_SIZE_IN_BYTES = 15*4
# addresses per packet
LOCATIONS_PER_PACKET = 1

ANOTATION_STR = "			"
HANDLED_FIXUPS = ["R_PRIMATE_BRANCH",
                  "R_PRIMATE_JAL"]

# pkt size in bytes
PACKET_SIZE = PACKET_SIZE_IN_BYTES / LOCATIONS_PER_PACKET

if len(sys.argv) != 4:
    print("use is ./fixup <file objdump -dr> <file of objdump -t> <output binary>")
    exit(-1)

fname = sys.argv[1]
symname = sys.argv[2]
oname = sys.argv[3]
print ("opening " + fname + " to prog")
print ("opening " + symname + " to syms")
print ("opening " + oname + " to write")

outFile = open(oname, "w+")

symPat = re.compile(r"[0-9a-f]{8} <..*:")
symTable = {}

def fix_last_branch(packet, location, instr_pc):
    target = symTable[location]
    offset = target - instr_pc
    offset = offset & 0xFFFFFFFF

    # print("fix up for branch:", packet[-1])
    # print("offset is:", str(offset))
    
    iToks = packet[-1].split()
    instr_val = 0
    for i in iToks[0:4][::-1]:
        instr_val = instr_val << 8
        instr_val += int(i, 16)
    added_val = 0
    if(iToks[4].startswith("j")):
        added_val += ((offset & (0x001 << 11)) >> 11) << 20
        added_val += ((offset & (0x3FF <<  1)) >>  1) << 21
        added_val += ((offset & (0x001 << 20)) >> 20) << 31
        added_val += ((offset & (0x0FF << 12)) >> 12) << 12
        
    elif(iToks[4].startswith("b")):
        added_val += ((offset & (0x03F <<  5)) >>  5) << 25
        added_val += ((offset & (0x00F <<  1)) >>  1) << 8
        added_val += ((offset & (0x001 << 11)) >> 11) << 7
        added_val += ((offset & (0x001 << 12)) >> 12) << 31

    instr_val += added_val
    # print("instr    ", hex(instr_val))
    # print("added_val", hex(added_val))

    newInstr = packet[-1].split()
    for i in range(4):
        newInstr[i] = "{:02x}".format(instr_val & 0xff)
        instr_val = instr_val >> 8
    newInstr[-1] = "<" + location + ">"
    packet[-1] = ' '.join(newInstr)
        
with open(symname) as f:
    for i in range(4):
        next(f)
    print("generating symbol table")
    for line in f:
        toks = line.strip().split()
        if int(toks[0], 16) % PACKET_SIZE != 0:
            print("BAD SYM: offset doesn't match packet_size")
            print(line)
            print("offset:", int(toks[0], 16) % PACKET_SIZE)
            print("Packet size:", PACKET_SIZE)
            exit(-1)
        addr = int(int(toks[0], 16) / PACKET_SIZE)
        sym = toks[-1]
        symTable[sym] = addr

with open(fname) as f:
    for i in range(4):
        next(f)
    currentPacket = []
    for line in f:
        if(line.startswith(ANOTATION_STR)):
            line = line.strip()
            line_address, rest = line.split(":")
            rest = rest.strip()
            if not rest.split()[0] in HANDLED_FIXUPS:
                print(f"unhandled fixup {line}")
                continue
        line = line.strip()
        if len(line) == 0:
            continue
        if symPat.match(line):
            pass
        else:
            try:
                line_address, rest = line.split(":")
                rest = rest.strip()
                
                line_address = int(int(line_address, 16) / PACKET_SIZE)
                if rest.split()[0].startswith("R_PRIMATE"):
                    loc = rest.split()[1]
                    fix_last_branch(currentPacket, loc, line_address)
                else:
                    currentPacket.append(rest)
            except ValueError as e:
                print("error line " + str(e))
                print(line)

        if len(currentPacket) == 10:
            #print(currentPacket)
            for instr in currentPacket:
                iToks = instr.split()
                instr_val = ""
                for i in iToks[0:4][::-1]:
                    instr_val += i.strip()
                outFile.write(instr_val)
            outFile.write("\n")
            currentPacket = []
            
