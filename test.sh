#!/bin/sh
echo "---  Assembling: machine/asm ---"
./build/asm ./build/out.s

echo "---  Running: machine/machine ---"
./build/machine ./build/out.o

echo "--- Done ---"
