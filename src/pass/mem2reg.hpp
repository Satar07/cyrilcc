#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include "type.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Mem2RegPhiInsertionPass : public FunctionPass {
  private:
    // 可提升的 alloca 映射 (Key: alloca_name, Value: alloca 分配的类型)
    std::unordered_map<std::string, IRType *> promotable_allocas;

    // 映射: 哪个 PHI 节点 对应哪个 Alloca
    std::unordered_map<std::string, std::string> phi_to_alloca_map;

    // 跟踪每个 alloca 的当前 SSA 值定义
    // (Key: alloca_name, Value: 一个定义栈)
    std::unordered_map<std::string, std::vector<IROperand>> def_map_stacks;

    // 跟踪 LOAD 结果的重命名
    // (Key: %load_result_name, Value: 替换它的 SSA 值)
    std::unordered_map<std::string, IROperand> rename_map;

    // 待删除的指令 (alloca, load, store)
    std::unordered_set<IRInstruction *> instructions_to_delete;

    /**
     * 分析函数 F，填充 promotable_allocas 映射。
     *
     * 一个 alloca 可被提升，当且仅当:它是标量 (非数组/结构体) or
     * 它的地址从未 "逃逸" (即它只被用于 LOAD 和 STORE)。
     */
    void analyze_allocas(IRFunction &F) {
        promotable_allocas.clear();
        auto &entry_block = F.blocks[0];

        std::vector<IRInstruction *> candidate_allocas;
        for (auto &inst : entry_block->insts) {
            if (inst.op == IROp::ALLOCA) {
                candidate_allocas.push_back(&inst);
            }
        }
        if (candidate_allocas.empty()) return;

        for (IRInstruction *alloca_inst : candidate_allocas) {
            const std::string &ptr_name = alloca_inst->result->name;
            IRType *allocated_type = alloca_inst->result->type->get_pointee_type();
            // 类型检查
            if (allocated_type->is_array() || allocated_type->is_struct()) {
                continue; // 此 alloca 不可提升
            }
            // 用法检查
            bool address_escapes = false;
            for (auto &block : F.blocks) {
                for (auto &inst : block->insts) {
                    for (auto &arg : inst.args) {
                        if (arg.op_type == IROperandType::REG && arg.name == ptr_name) {
                            if (inst.op == IROp::LOAD and &arg == &inst.args[0]) {
                                continue;
                            }
                            if (inst.op == IROp::STORE and &arg == &inst.args[1]) {
                                continue;
                            }
                            address_escapes = true;
                            break;
                        }
                    }
                    if (address_escapes) break;
                }
                if (address_escapes) break;
            }

            if (!address_escapes) {
                this->promotable_allocas[ptr_name] = allocated_type;
            }
        }
    }

    void insert_phiNodes(IRFunction &F) {
        for (auto const &[alloca_name, var_type] : promotable_allocas) {
            std::unordered_set<IRBasicBlock *> has_phi_inserted;
            std::unordered_set<IRBasicBlock *> def_blocks;
            for (auto &block : F.blocks) {
                for (auto &inst : block->insts) {
                    if (inst.op == IROp::STORE) {
                        if (inst.args[1].op_type == IROperandType::REG &&
                            inst.args[1].name == alloca_name) {
                            def_blocks.insert(block.get());
                        }
                    }
                }
            }
            std::vector<IRBasicBlock *> work_list(def_blocks.begin(), def_blocks.end());
            while (!work_list.empty()) {
                IRBasicBlock *d = work_list.back();
                work_list.pop_back();
                for (IRBasicBlock *b : d->dom_frontiers) {
                    if (not has_phi_inserted.contains(b)) {
                        IROperand res = F.new_reg(var_type); // 定义新SSA变量给phi节点
                        IRInstruction phi_inst(IROp::PHI, {}, res);
                        b->insts.insert(++b->insts.begin(), phi_inst);
                        has_phi_inserted.insert(b);
                        work_list.push_back(b);
                        phi_to_alloca_map.insert({ res.name, alloca_name });
                    }
                }
            }
        }
    }

    void init_def_map_stack(IRFunction &F) {
        for (auto const &[alloca_name, var_type] : promotable_allocas) {
            // 查找 entry0 中的初始 store
            IROperand initial_val;
            bool found_store = false;
            for (auto &inst : F.blocks[0]->insts) {
                if (inst.op == IROp::STORE && inst.args[1].name == alloca_name) {
                    initial_val = inst.args[0];
                    found_store = true;
                    // 我们还必须删除这个初始 store
                    instructions_to_delete.insert(&inst);
                    break;
                }
            }

            if (!found_store) {
                // 未初始化
                initial_val = IROperand::create_imm(0, var_type);
            }
            def_map_stacks[alloca_name].push_back(initial_val);
        }
    }

    void rename_recursive(IRBasicBlock *B) {
        // 跟踪在此块中推入
        // (Key: alloca_name, Value: 推入次数)
        std::unordered_map<std::string, int> definitions_pushed_count;
        // 跟踪在此块中定义的 LOAD 结果
        std::vector<std::string> load_results_defined_in_this_block;

        for (auto &inst : B->insts) {
            // 重命名使用
            if (inst.op != IROp::PHI) {
                for (auto &arg : inst.args) {
                    if (rename_map.contains(arg.name)) {
                        arg = rename_map.at(arg.name);
                    }
                }
            }

            if (inst.op == IROp::ALLOCA) {
                if (promotable_allocas.contains(inst.result->name)) {
                    instructions_to_delete.insert(&inst);
                }
                continue;
            }
            if (inst.op == IROp::PHI) {
                std::string alloca_name = phi_to_alloca_map.at(inst.result->name);
                auto new_def = inst.result.value();
                def_map_stacks[alloca_name].push_back(new_def);
                definitions_pushed_count[alloca_name]++;
                continue;
            }
            if (inst.op == IROp::LOAD) {
                std::string alloca_name = inst.args[0].name;
                if (promotable_allocas.contains(alloca_name)) {
                    IROperand current_def = def_map_stacks[alloca_name].back();

                    // 将 LOAD 的结果 (%load_res) 映射到该 SSA 值
                    rename_map[inst.result->name] = current_def;

                    // 记录下来，以便在退出此块时撤销
                    load_results_defined_in_this_block.push_back(inst.result->name);
                    instructions_to_delete.insert(&inst);
                }
                // continue;
            }
            if (inst.op == IROp::STORE) {
                // auto &value = inst.args[0];
                // if (IROperandType::REG != value.op_type) continue;
                std::string alloca_name = inst.args[1].name;
                if (promotable_allocas.count(alloca_name)) {
                    // 这是一个新定义
                    IROperand value_to_store = inst.args[0];
                    def_map_stacks[alloca_name].push_back(
                        value_to_store); // 推入新定义，让后续phi节点用这个
                    definitions_pushed_count[alloca_name]++;
                    instructions_to_delete.insert(&inst);
                }
            }
        }

        // 填充后继块的 PHI 节点
        for (IRBasicBlock *S : B->successors) {
            for (auto &succ_inst : S->insts) {
                if (succ_inst.op == IROp::LABEL) continue; // 跳过标签指令
                if (succ_inst.op != IROp::PHI) break;      // 只处理开头的一系列 PHI 节点
                std::string alloca_name = phi_to_alloca_map.at(succ_inst.result->name);
                if ((not def_map_stacks.contains(alloca_name)) or
                    def_map_stacks.at(alloca_name).empty()) {
                    throw std::runtime_error("Def stack is empty when filling PHI nodes");
                    continue;
                    // def_map_stacks.at(alloca_name)
                    //     .push_back(IROperand::create_imm(0, IRType::get_i32()));
                }

                IROperand value_from_this_block = def_map_stacks.at(alloca_name).back();
                succ_inst.args.push_back(value_from_this_block);
                succ_inst.args.push_back(IROperand::create_label(B->label));
            }
        }

        // 递归支配树
        for (IRBasicBlock *C : B->dom_child) {
            rename_recursive(C);
        }

        for (auto const &[alloca_name, count] : definitions_pushed_count) {
            for (int i = 0; i < count; i++) {
                def_map_stacks[alloca_name].pop_back();
            }
        }
        for (const auto &old_vreg : load_results_defined_in_this_block) {
            rename_map.erase(old_vreg);
        }
    }

    // 从 IR 中真正删除"已死亡"的指令
    void cleanup_instructions(IRFunction &F) {
        for (auto &block : F.blocks) {
            auto &insts = block->insts;
            insts.erase(std::remove_if(insts.begin(), insts.end(),
                                       [&](const IRInstruction &inst) {
                                           return instructions_to_delete.count(
                                               const_cast<IRInstruction *>(&inst));
                                       }),
                        insts.end());
        }
    }

  public:
    bool run(IRFunction &F) override {
        if (F.blocks.empty()) return false;

        // 找出哪些 alloca 可以提升
        analyze_allocas(F);
        if (promotable_allocas.empty()) {
            return false;
        }

        F.dump(std::cout);

        // 插入 PHI 节点
        insert_phiNodes(F);

        F.dump(std::cout);

        init_def_map_stack(F);

        // 递归重命名
        rename_recursive(F.blocks[0].get());

        F.dump(std::cout);

        // 清理
        cleanup_instructions(F);

        return true;
    }
};
