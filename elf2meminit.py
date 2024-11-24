#! /bin/python3
import os
import re
import sys

if len(sys.argv) != 3: 
    print("wrong number of arguments....")
    print("Expected: " + sys.argv[0] + "<.rodata section dump> <output memInit.txt>")
    exit(-1)

out_file = open(sys.argv[2], "w+")
with open(sys.argv[1]) as f:
    lines = f.readlines()
    for line in lines[3:]:
        line = line.strip()
        toks = line.split()[1:5]
        print(toks)
        mask = 0x0ff
        shift_amt = 24
        for tok in toks:
            try: 
                tok_int = int(tok, 16)
            except ValueError:
                print("ERROR:", tok)
                continue
            
            print(hex((tok_int & (mask << shift_amt)) >> shift_amt)[2:], file=out_file)
            
            shift_amt -= 8
out_file.close()
        
        

