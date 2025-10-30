#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm_gen.hpp"
#include "ast.hpp"
#include "ir.hpp"
#include "lexer.h"
#include "parser.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s filename\n", argv[0]);
        exit(0);
    }

    if ((yyin = fopen(argv[1], "r")) == NULL) {
        printf("open file %s failed\n", argv[1]);
        exit(0);
    }

    // 调用解析器 生成AST到root
    yyparse();

    if (root) {
        // 打印 AST
        root.get()->print(std::cout);
        // 生成 IR
        IRGenerator ir{ root };
        ir.dump_ir();
        AsmGenerator asm_gen{ ir.module, std::cout };
        asm_gen.generate();
    }

    fclose(yyin);
    return 0;
}
