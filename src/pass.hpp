// pass.hpp
#pragma once

#include "ir.hpp" // Pass 需要操作 IR
#include <iostream>
#include <memory>
#include <vector>

class FunctionPass {
  public:
    virtual ~FunctionPass() = default;

    /**
     * @brief 在单个函数上运行此 Pass
     * @param F 要操作的函数
     * @return true 如果 Pass 修改了 IR，否则返回 false
     */
    virtual bool run(IRFunction &F) = 0;
};

class ModulePass {
  public:
    virtual ~ModulePass() = default;

    /**
     * @brief 在整个模块上运行此 Pass
     * @param M 要操作的模块
     * @return true 如果 Pass 修改了 IR，否则返回 false
     */
    virtual bool run(IRModule &M) = 0;
};

class PassManager {
  private:
    std::vector<std::unique_ptr<FunctionPass>> function_passes;
    std::vector<std::unique_ptr<ModulePass>> module_passes;

  public:
    void addFunctionPass(FunctionPass *pass) {
        function_passes.emplace_back(pass);
    }

    void addModulePass(ModulePass *pass) {
        module_passes.emplace_back(pass);
    }

    void run(IRModule &M) {
        // 运行所有 Module Pass
        for (auto &pass : module_passes) {
            pass->run(M);
        }

        // 在每个 Function 上运行所有 Function Pass
        for (IRFunction &F : M.functions) {
            for (auto &pass : function_passes) {
                pass->run(F);
                M.dump(std::cout);
            }
        }
    }
};
