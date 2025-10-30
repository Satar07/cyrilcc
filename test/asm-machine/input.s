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
   SUB R12, 44             # Allocate stack frame
L0:
   ITI
   LOD R8, R15             # Move %2 (cached)
   STO (R11 - 4), R8       # Store to alloca
   LOD R8, 99              # Load immediate char
   STC (R11 - 8), R8       # Store to alloca
   LOD R8, (R11 - 4)       # Load from alloca
   LOD R9, 1               # Load immediate
   SUB R8, R9              # L - R
   TST R8
   # Spilling all regs: BRZ
   STO (R11 - 16), R8      # Spill %3
   STO (R11 - 12), R15     # Spill %2
   JEZ L1
L3:
   JMP L2
L1:
   LOD R8, 100             # Load immediate char
   STC (R11 - 8), R8       # Store to alloca
   JMP L2
L2:
   JMP L4
L4:
   LOD R8, (R11 - 4)       # Load from alloca
   LOD R9, 1               # Load immediate
   SUB R8, R9              # L - R
   TST R8
   # Spilling all regs: BRGT
   STO (R11 - 20), R8      # Spill %4
   JGZ L5
L7:
   JMP L6
L5:
   LOD R8, 101             # Load immediate char
   STC (R11 - 8), R8       # Store to alloca
   JMP L6
   JMP L4
L6:
   LOD R8, (R11 - 4)       # Load from alloca
   # Spilling all regs: Call
   STO (R11 - 24), R8      # Spill %5
   LOD R2, (R11 - 24)      # Reload %5 from home
   LOD R3, 3               # Load immediate
   LOD R14, LL0            # Set return address
   JMP FUNCmax             # Call function
LL0:
   LOD R8, R2              # Move %6 (cached)
   LOD R9, 123             # Load immediate
   LOD R10, R8             # Move L to Dest
   ADD R10, R9             # Binary op
   LOD R8, R10             # Move %7 (cached)
   STO (R11 - 4), R8       # Store to alloca
   LDC R8, (R11 - 8)       # Load from alloca
   LOD R15, R8             # Move %8 (cached)
   OTC
   STO (R11 - 36), R8      # Spill %8
   LOD R8, (R11 - 4)       # Load from alloca
   LOD R9, VARglobalint    # Load global addr
   STO (R9), R8            # Store to global var
   STO (R11 - 40), R8      # Spill %9
   LDC R8, (R11 - 8)       # Load from alloca
   LOD R9, VARglobalchar   # Load global addr
   STC (R9), R8            # Store to global var
   LOD R2, 0               # Load immediate
   LOD R12, R11            # Restore SP
   LOD R14, (R12 + 4)      # Pop RA
   LOD R11, (R12 + 8)      # Pop old FP
   ADD R12, 8              # Cleanup stack
   JMP R14                 # Return

# --- Function: @max ---
FUNCmax:
   STO (R12), R11          # Push old FP
   SUB R12, 4
   STO (R12), R14          # Push return address (RA)
   SUB R12, 4
   LOD R11, R12            # FP = new SP
   SUB R12, 32             # Allocate stack frame
   STO (R11 - 4), R2       # Store param %0 to home
   STO (R11 - 8), R3       # Store param %2 to home
L8:
   LOD R8, (R11 - 4)       # Reload %0 from home
   STO (R11 - 12), R8      # Store to alloca
   LOD R8, (R11 - 8)       # Reload %2 from home
   STO (R11 - 16), R8      # Store to alloca
   LOD R8, (R11 - 12)      # Load from alloca
   STO (R11 - 20), R8      # Spill %4
   LOD R8, (R11 - 16)      # Load from alloca
   LOD R8, (R11 - 20)      # Reload %4 from home
   LOD R9, R8              # Move %5 (cached)
   SUB R8, R9              # L - R
   TST R8
   # Spilling all regs: BRLT
   STO (R11 - 24), R8      # Spill %5
   JLZ L9
L12:
   JMP L11
L9:
   LOD R8, (R11 - 16)      # Load from alloca
   LOD R2, R8              # Move %6 (cached)
   LOD R12, R11            # Restore SP
   LOD R14, (R12 + 4)      # Pop RA
   LOD R11, (R12 + 8)      # Pop old FP
   ADD R12, 8              # Cleanup stack
   JMP R14                 # Return
   # Spilling all regs: BR
   STO (R11 - 28), R8      # Spill %6
   JMP L10
L11:
   LOD R8, (R11 - 12)      # Load from alloca
   LOD R2, R8              # Move %7 (cached)
   LOD R12, R11            # Restore SP
   LOD R14, (R12 + 4)      # Pop RA
   LOD R11, (R12 + 8)      # Pop old FP
   ADD R12, 8              # Cleanup stack
   JMP R14                 # Return
   # Spilling all regs: BR
   STO (R11 - 32), R8      # Spill %7
   JMP L10
L10:
   LOD R2, 0               # Load immediate
   LOD R12, R11            # Restore SP
   LOD R14, (R12 + 4)      # Pop RA
   LOD R11, (R12 + 8)      # Pop old FP
   ADD R12, 8              # Cleanup stack
   JMP R14                 # Return

# --- Data Segment ---
VARglobalint:
   DBN 0, 4                # Global var: @globalint
VARglobalchar:
   DBN 0, 4                # Global var: @globalchar
