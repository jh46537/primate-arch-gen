#! /bin/python3
import os
import re
import sys
import math

if len(sys.argv) != 5: 
    print("wrong number of arguments....")
    print("Expected: " + sys.argv[0] + "<file objdump -dr> <file of objdump -t> <primate.cfg> <output binary>")
    exit(-1)

config_name = sys.argv[3]
with open(config_name) as f:
    for line in f:
        toks = line.split("=")
        if toks[0] == "NUM_ALUS":
            numALUs = int(toks[1])
        if toks[0] == "NUM_BFUS":
            numBFUs = int(toks[1]) + 2 # IO and memory unit is implicit
        if toks[0] == "NUM_REGS":
            num_regs = int(toks[1])
            num_regs_lg = int(math.ceil(math.log2(num_regs)))

numSlots = max(numBFUs, numALUs)

# BFUs and ALUs are merged starting with the last BFU slot. 
if numALUs >= numBFUs:
  hasGFU = [True] * numSlots
  hasBFU = [True] * (numBFUs) + [False] * (numSlots-numBFUs)
else:
  hasGFU = [True] * (numALUs) + [False] * (numBFUs - numALUs)
  hasBFU = [True] * numSlots

PACKET_SIZE_IN_INSTRS = 1 # branch unit
for g, b in zip(hasGFU, hasBFU):
    if g:
        PACKET_SIZE_IN_INSTRS += 4 # unit + ext, ext, ins
    elif b:
        PACKET_SIZE_IN_INSTRS += 1 # unit
    else:
        print("something has gone wrong in backend mapping (slot is not green nor blue)")
        exit(-1)
  
# pkt size in bytes
SUBINSTR_SIZE_BYTES = int(math.ceil((num_regs_lg * 3 + 7 + 3 + 7)/8))
PACKET_SIZE_IN_BYTES = PACKET_SIZE_IN_INSTRS*SUBINSTR_SIZE_BYTES
# addresses per packet
LOCATIONS_PER_PACKET = 4

NOP_STR = "00000013"
RET_STR = "fffff06f"
ANOTATION_STR = "			"
HANDLED_FIXUPS = ["R_PRIMATE_BRANCH",
                  "R_PRIMATE_JAL"]

fname = sys.argv[1]
symname = sys.argv[2]
oname = sys.argv[4]
print ("opening " + fname + " to prog")
print ("opening " + symname + " to syms")
print ("opening " + oname + " to write")

outFile = open(oname, "w+")


symPat = re.compile(r"[0-9a-f]{8} <.*:")
pktBrk = re.compile(r"[0-9]+ --------$")
symTable = {}

def write_packet(packet):
    for instr in currentPacket[::-1]:
        iToks = instr.split()
        instr_val = ""
        for i in iToks[0:SUBINSTR_SIZE_BYTES][::-1]:
            instr_val += i.strip()
        # print(instr_val)
        outFile.write(instr_val)
    outFile.write("\n")

def fix_last_branch(packet):
    return
    iToks = packet[-1].split()
    instr_val = 0
    for i in iToks[0:4][::-1]:
        instr_val = instr_val << 8
        instr_val += int(i, 16)
    added_val = 0

    offset = 0
    # get offset
    if(iToks[4].startswith("j")):
        offset += ((instr_val & (0x001 << 20)) >> 20) << 11
        offset += ((instr_val & (0x3FF << 21)) >> 21) <<  1
        offset += ((instr_val & (0x001 << 31)) >> 31) << 20
        offset += ((instr_val & (0x0FF << 12)) >> 12) << 12
    elif(iToks[4].startswith("b")):
        offset += ((instr_val & (0x03F << 25)) >> 25) <<  5
        offset += ((instr_val & (0x00F <<  8)) >>  8) <<  1
        offset += ((instr_val & (0x001 <<  7)) >>  7) << 11
        offset += ((instr_val & (0x001 << 31)) >> 31) << 12
    
    offset = int(offset / PACKET_SIZE_IN_BYTES)
    #print(f"normal {instr_val}")
    
    # zero offset 
    if(iToks[4].startswith("j")):
        instr_val = (instr_val & ~(0x001 << 20))
        instr_val = (instr_val & ~(0x3FF << 21))
        instr_val = (instr_val & ~(0x001 << 31))
        instr_val = (instr_val & ~(0x0FF << 12))
    elif(iToks[4].startswith("b")):
        instr_val = (instr_val & ~(0x03F << 25))
        instr_val = (instr_val & ~(0x00F <<  8))
        instr_val = (instr_val & ~(0x001 <<  7))
        instr_val = (instr_val & ~(0x001 << 31))
    #print(f"zerod {hex(instr_val)}")
    
    # patch in the offset
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
    
    # Primate compiler now handles relocs
    instr_val += added_val
    #print("instr    ", hex(instr_val))
    #print("added_val", hex(added_val))
    
    newInstr = packet[-1].split()
    for i in range(4):
        newInstr[i] = "{:02x}".format(instr_val & 0xff)
        instr_val = instr_val >> 8
    newInstr[-1] = "<" + " this is blah blah " + ">"
    packet[-1] = ' '.join(newInstr)

print("starting with backend config:")
print(f"hasGFU: {hasGFU}")
print(f"hasBFU: {hasBFU}")
found_main = False
with open(fname) as f:
    for i in range(4):
        next(f)
    currentPacket = []
    for line in f:
        if "primate_main" in line and not found_main:
            print(f"found main: {line}")
            found_main = True
        if not found_main:
            continue
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
        elif pktBrk.match(line):
            pass
        else:
            try:
                line_address, rest = line.split(":")
                rest = rest.strip()
                
                line_address = int(int(line_address, 16) / PACKET_SIZE_IN_BYTES)
                # if rest.split()[0].startswith("R_PRIMATE"):
                #     loc = rest.split()[1]
                #     fix_last_branch(currentPacket, loc, line_address)
                # else:
                if len(currentPacket) == PACKET_SIZE_IN_INSTRS:
                    write_packet(currentPacket)
                    currentPacket = []
                currentPacket.append(rest)
            except ValueError as e:
                print("error line " + str(e))
                print(line)
    assert(len(currentPacket) == PACKET_SIZE_IN_INSTRS)
    write_packet(currentPacket)
