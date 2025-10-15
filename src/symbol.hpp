#pragma once

typedef union {
    int num;
    char c;
} SymbolValue;


void declare_symbol(char const *name);
void store_symbol(char const *name, SymbolValue value);
SymbolValue load_symbol(char const *name);
