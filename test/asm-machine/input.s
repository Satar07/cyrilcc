# --- Text Segment ---
    LOD R12, 65535          # Init Stack Pointer
    LOD R11, R12            # Init Frame Pointer
    LOD R14, EXIT           # main func ret point
    JMP FUNCmain            # Jump to main function
EXIT:
    END

# --- Function: @main ---
FUNCmain:
    STO (R12), R11          # Push old FP
    SUB R12, 4
    STO (R12), R14          # Push return address (RA)
    SUB R12, 4
    LOD R11, R12            # FP = new SP
    SUB R12, 60             # Allocate stack frame
entry:
    ITI
    LOD R8, R15             # Move %2 (cached)
    STO (R11 - 8), R8       # Store to alloca
    STO (R11 - 12), R8      # Spill %2 (load imm)
    LOD R8, 0               # Load immediate
    STO (R11 - 4), R8       # Store to alloca
    JMP forcond0
forcond0:
    LOD R8, (R11 - 4)       # Load from alloca
    STO (R11 - 16), R8      # Spill %3 (assign)
    LOD R8, (R11 - 8)       # Load from alloca
    STO (R11 - 20), R8      # Spill %4 (load home)
    LOD R8, (R11 - 16)      # Reload %3 from home
    LOD R9, (R11 - 20)      # Reload %4 from home
    SUB R8, R9              # L - R
    TST R8
    # Spilling all regs: BRLT
    STO (R11 - 20), R9      # Spill %4
    STO (R11 - 16), R8      # Spill %3
    JLZ forbody1
    JMP forend3
forbody1:
    LOD R8, (R11 - 4)       # Load from alloca
    LOD R9, 10              # Load immediate
    SUB R8, R9              # L - R
    TST R8
    # Spilling all regs: BRGT
    STO (R11 - 24), R8      # Spill %5
    JGZ iftrue4
    JMP ifend5
iftrue4:
    LOD R15, STRstr0        # Load global/label addr
    OTS
    JMP forend3
unreachable6:
    JMP ifend5
ifend5:
    LOD R8, (R11 - 4)       # Load from alloca
    LOD R15, R8             # Move %6 (cached)
    OTI
    # Spilling all regs: BR
    STO (R11 - 28), R15     # Spill %6
    JMP forinc2
forinc2:
    LOD R8, (R11 - 4)       # Load from alloca
    LOD R9, 1               # Load immediate
    LOD R10, R8             # Move L to Dest
    ADD R10, R9             # Binary op
    STO (R11 - 32), R8      # Spill %7 (move reg)
    LOD R8, R10             # Move %8 (cached)
    STO (R11 - 4), R8       # Store to alloca
    # Spilling all regs: BR
    STO (R11 - 36), R8      # Spill %8
    JMP forcond0
forend3:
    LOD R15, STRstr1        # Load global/label addr
    OTS
    LOD R8, 0               # Load immediate
    STO (R11 - 4), R8       # Store to alloca
    JMP forcond7
forcond7:
    LOD R8, (R11 - 4)       # Load from alloca
    STO (R11 - 40), R8      # Spill %9 (assign)
    LOD R8, (R11 - 8)       # Load from alloca
    STO (R11 - 44), R8      # Spill %10 (load home)
    LOD R8, (R11 - 40)      # Reload %9 from home
    LOD R9, (R11 - 44)      # Reload %10 from home
    SUB R8, R9              # L - R
    TST R8
    # Spilling all regs: BRLT
    STO (R11 - 44), R9      # Spill %10
    STO (R11 - 40), R8      # Spill %9
    JLZ forbody8
    JMP forend10
forbody8:
    LOD R8, (R11 - 4)       # Load from alloca
    LOD R9, 10              # Load immediate
    SUB R8, R9              # L - R
    TST R8
    # Spilling all regs: BRZ/BREQ
    STO (R11 - 48), R8      # Spill %11
    JEZ iftrue11
    JMP ifend12
iftrue11:
    LOD R15, STRstr2        # Load global/label addr
    OTS
    JMP forinc9
unreachable13:
    JMP ifend12
ifend12:
    LOD R8, (R11 - 4)       # Load from alloca
    LOD R15, R8             # Move %12 (cached)
    OTI
    # Spilling all regs: BR
    STO (R11 - 52), R15     # Spill %12
    JMP forinc9
forinc9:
    LOD R8, (R11 - 4)       # Load from alloca
    LOD R9, 1               # Load immediate
    LOD R10, R8             # Move L to Dest
    ADD R10, R9             # Binary op
    STO (R11 - 56), R8      # Spill %13 (move reg)
    LOD R8, R10             # Move %14 (cached)
    STO (R11 - 4), R8       # Store to alloca
    # Spilling all regs: BR
    STO (R11 - 60), R8      # Spill %14
    JMP forcond7
forend10:
    LOD R15, STRstr3        # Load global/label addr
    OTS
    LOD R12, R11            # Restore SP
    LOD R14, (R12 + 4)      # Pop RA
    LOD R11, (R12 + 8)      # Pop old FP
    ADD R12, 8              # Cleanup stack
    JMP R14                 # Return

# --- Data Segment ---
STRstr0:
    DBS 98, 114, 101, 97, 107, 0# String: @str0
STRstr1:
    DBS 92, 110, 0          # String: @str1
STRstr2:
    DBS 99, 111, 110, 116, 105, 110, 117, 101, 0# String: @str2
STRstr3:
    DBS 92, 110, 0          # String: @str3
