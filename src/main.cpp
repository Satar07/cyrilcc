#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "asm_gen.hpp"
#include "ast.hpp"
#include "ir.hpp"
#include "lexer.h"
#include "parser.h"
#include "pass.hpp"
#include "pass/deSSA.hpp"
#include "pass/dom_analysis.hpp"
#include "pass/mem2reg.hpp"
#include "pass/sccp.hpp"

int main(int argc, char *argv[]) {
    const char *input_path = nullptr;
    const char *asm_output_path = nullptr;

    // cyrilcc input.m -o output.s
    if (argc == 4 && std::string(argv[2]) == "-o") {
        input_path = argv[1];
        asm_output_path = argv[3];
    } else {
        fprintf(stderr, "Usage: %s <input.m> -o <output.s>\n", argv[0]);
        exit(1);
    }

    if ((yyin = fopen(input_path, "r")) == nullptr) {
        fprintf(stderr, "Error: open file %s failed\n", input_path);
        exit(1);
    }

    // 调用解析器 生成AST到root
    yyparse();

    if (root) {
        root->print(std::cout);

        IRGenerator ir{ root };

        PassManager pm;
        pm.addFunctionPass(new BuildCFGPass());
        pm.addFunctionPass(new DeadBlockEliminationPass());
        pm.addFunctionPass(new DominatorTreePass());
        pm.addFunctionPass(new DominanceFrontierPass());
        pm.addFunctionPass(new DataFlowAnalysisPass());

        pm.addFunctionPass(new Mem2RegPhiInsertionPass());
        pm.addFunctionPass(new DataFlowAnalysisPass()); // 更新

        pm.addFunctionPass(new SCCPPass());

        pm.addFunctionPass(new DeSSAPass());

        pm.run(ir.module);

        ir.module.dump(std::cout);

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
