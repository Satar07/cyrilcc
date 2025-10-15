#include "symbol.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <iostream>

void yyerror(const char *msg);

typedef std::unordered_map<std::string, SymbolValue> SymbolMap;

SymbolMap symbol_map;

void declare_symbol(char const *name) {
    std::string key(name);
    if (symbol_map.find(key) != symbol_map.end()) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Symbol already declared: %s", name);
        yyerror(buf);
        return;
    }
    // symbol_map[key] = value;
}

void store_symbol(char const *name, SymbolValue value) {
    std::string key(name);
    symbol_map[key] = value;
    std::cout<<"Store symbol: " << name << " = " << value.num << std::endl;
}

SymbolValue load_symbol(char const *name) {
    std::string key(name);
    auto it = symbol_map.find(key);
    if (it != symbol_map.end()) {
        return it->second;
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "Symbol not found: %s", name);
        yyerror(buf);
        return SymbolValue();
    }
}