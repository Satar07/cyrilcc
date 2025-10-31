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
   SUB R12, 28             # Allocate stack frame
L0:
   LOD R8, 3               # Load immediate
   STO (R11 - 4), R8       # Store to alloca
   LOD R10, (R11 - 4)      # Load from alloca
   LOD R8, R10             # Move %1 (cached)
   LOD R9, 1               # Load immediate
   SUB R8, R9              # L - R
   TST R8
   # Spilling all regs: BRZ
   STO (R11 - 8), R10      # Spill %1
   JEZ L2
L5:
   LOD R8, (R11 - 8)       # Reload %1 from home
   LOD R9, 2               # Load immediate
   SUB R8, R9              # L - R
   TST R8
   JEZ L4
L6:
   JMP L3
L2:
   LOD R10, (R11 - 4)      # Load from alloca
   LOD R8, R10             # Move %2 (cached)
   LOD R9, 1               # Load immediate
   STO (R11 - 12), R10     # Spill %2
   LOD R10, R8             # Move L to Dest
   ADD R10, R9             # Binary op
   LOD R8, R10             # Move %3 (cached)
   STO (R11 - 4), R8       # Store to alloca
   JMP L1
L3:
   LOD R8, 0               # Load immediate
   STO (R11 - 4), R8       # Store to alloca
L4:
   STO (R11 - 16), R10     # Spill %3
   LOD R10, (R11 - 4)      # Load from alloca
   LOD R8, R10             # Move %4 (cached)
   LOD R9, 2               # Load immediate
   STO (R11 - 20), R10     # Spill %4
   LOD R10, R8             # Move L to Dest
   ADD R10, R9             # Binary op
   LOD R8, R10             # Move %5 (cached)
   STO (R11 - 4), R8       # Store to alloca
   JMP L1
L1:
   STO (R11 - 24), R10     # Spill %5
   LOD R10, (R11 - 4)      # Load from alloca
   LOD R15, R10            # Move %6 (cached)
   OTI
   LOD R2, 0               # Load immediate
   LOD R12, R11            # Restore SP
   LOD R14, (R12 + 4)      # Pop RA
   LOD R11, (R12 + 8)      # Pop old FP
   ADD R12, 8              # Cleanup stack
   JMP R14                 # Return

# --- Data Segment ---
