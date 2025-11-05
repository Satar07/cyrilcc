#include <fstream>
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
        root.get()->print(std::cout);

        IRGenerator ir{ root };
        ir.module.dump(std::cout);

        const char *asm_output_path = "asm-machine/input.s";
        std::ofstream asm_file_stream(asm_output_path);

        if (!asm_file_stream.is_open()) {
            fprintf(stderr, "Error: failed to open output file %s\n", asm_output_path);
            exit(1);
        }

        AsmGenerator asm_gen{ ir.module, asm_file_stream };
        asm_gen.generate();
    }

    fclose(yyin);
    return 0;
}
