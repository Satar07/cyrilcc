#!/bin/sh
cd asm-machine
echo "---  Assembling: machine/asm ---"
./asm input.s

echo "---  Running: machine/machine ---"
./machine input.o

echo "--- Done ---"
