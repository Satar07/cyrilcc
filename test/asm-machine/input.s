
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
    SUB R12, 56             # Allocate stack frame
entry:
    ITC
    LOD R8, R15             # Move %6 (cached)
    STC (R11 - 4), R8       # Store to alloca
    ITI
    STO (R11 - 28), R8      # Spill %6 (move reg)
    LOD R8, R15             # Move %7 (cached)
    STO (R11 - 16), R8      # Store to alloca
    STO (R11 - 32), R8      # Spill %7 (load imm)
    LOD R8, 0               # Load immediate
    LOD R9, 0               # Load immediate
    SUB R8, R9              # L - R
    TST R8
    JEZ ifend1
    JMP iftrue0
iftrue0:
    LOD R15, STRstr0        # Load global/label addr
    OTS
    JMP ifend1
ifend1:
    LOD R8, R11 - 4         # Get address of %0
    STO (R11 - 8), R8       # Store to alloca
    LOD R8, R11 - 16        # Get address of %3
    STO (R11 - 20), R8      # Store to alloca
    LOD R8, (R11 - 8)       # Load from alloca
    STO (R11 - 36), R8      # Spill %8 (assign)
    LOD R9, (R11 - 36)      # Reload %8 from home
    LDC R8, (R9)            # Load from pointer
    STC (R11 - 12), R8      # Store to alloca
    STO (R11 - 40), R8      # Spill %9 (assign)
    LOD R8, (R11 - 20)      # Load from alloca
    STO (R11 - 44), R8      # Spill %10 (assign)
    STO (R11 - 36), R9      # Spill %8 (load home)
    LOD R9, (R11 - 44)      # Reload %10 from home
    LOD R8, (R9)            # Load from pointer
    STO (R11 - 24), R8      # Store to alloca
    STO (R11 - 48), R8      # Spill %11 (load imm)
    LOD R8, 0               # Load immediate
    STO (R11 - 44), R9      # Spill %10 (load imm)
    LOD R9, 0               # Load immediate
    SUB R8, R9              # L - R
    TST R8
    JEZ ifend3
    JMP iftrue2
iftrue2:
    LOD R15, STRstr1        # Load global/label addr
    OTS
    JMP ifend3
ifend3:
    LDC R8, (R11 - 12)      # Load from alloca
    LOD R15, R8             # Move %12 (cached)
    OTC
    LOD R8, (R11 - 24)      # Load from alloca
    STO (R11 - 52), R15     # Spill %12 (move reg)
    LOD R15, R8             # Move %13 (cached)
    OTI
    STO (R11 - 56), R15     # Spill %13 (load addr)
    LOD R15, STRstr2        # Load global/label addr
    OTS
    LOD R12, R11            # Restore SP
    LOD R14, (R12 + 4)      # Pop RA
    LOD R11, (R12 + 8)      # Pop old FP
    ADD R12, 8              # Cleanup stack
    JMP R14                 # Return

# --- Data Segment ---
STRstr0:
    DBS 92, 110, 0          # String: @str0
STRstr1:
    DBS 92, 110, 0          # String: @str1
STRstr2:
    DBS 92, 110, 0          # String: @str2
