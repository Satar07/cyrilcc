#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 循环信息结构
struct LoopInfo {
    IRBasicBlock *header;                           // 循环头
    std::unordered_set<IRBasicBlock *> blocks;      // 循环体中的所有块
    std::unordered_set<IRBasicBlock *> exit_blocks; // 循环的退出块
    IRBasicBlock *preheader = nullptr;              // 预头块(用于放置外提的指令)
    LoopInfo *parent = nullptr;                     // 父循环
    std::vector<LoopInfo *> sub_loops;              // 子循环

    LoopInfo(IRBasicBlock *h) : header(h) {}
};

// 循环不变式外提 Pass
class LICMPass : public FunctionPass {
  private:
    IRFunction *current_function = nullptr;
    std::vector<std::unique_ptr<LoopInfo>> all_loops;
    std::unordered_map<IRBasicBlock *, LoopInfo *> block_to_loop;

    // 检测自然循环
    void detect_loops(IRFunction &F) {
        all_loops.clear();
        block_to_loop.clear();

        // 找到所有回边 (back edge): n -> d，其中 d 支配 n
        std::vector<std::pair<IRBasicBlock *, IRBasicBlock *>> back_edges;
        for (auto &block : F.blocks) {
            for (auto succ : block->successors) {
                if (dominates(succ, block.get())) {
                    back_edges.push_back({ block.get(), succ });
                }
            }
        }

        // 为每条回边构建自然循环
        for (auto [tail, head] : back_edges) {
            auto loop = std::make_unique<LoopInfo>(head);
            loop->blocks.insert(head);

            // 从尾节点回溯到头节点，收集所有循环体块
            std::vector<IRBasicBlock *> worklist = { tail };
            std::unordered_set<IRBasicBlock *> visited = { head };

            while (!worklist.empty()) {
                IRBasicBlock *current = worklist.back();
                worklist.pop_back();

                if (visited.count(current)) continue;
                visited.insert(current);
                loop->blocks.insert(current);

                for (auto pred : current->predecessors) {
                    if (!visited.count(pred)) {
                        worklist.push_back(pred);
                    }
                }
            }

            // 记录块到循环的映射
            for (auto block : loop->blocks) {
                block_to_loop[block] = loop.get();
            }

            all_loops.push_back(std::move(loop));
        }

        // 找出循环的退出块
        for (auto &loop : all_loops) {
            for (auto block : loop->blocks) {
                for (auto succ : block->successors) {
                    if (!loop->blocks.count(succ)) {
                        loop->exit_blocks.insert(succ);
                    }
                }
            }
        }
    }

    // 检查块 a 是否支配块 b
    bool dominates(IRBasicBlock *a, IRBasicBlock *b) {
        if (a == b) return true;
        IRBasicBlock *idom = b->idom;
        while (idom) {
            if (idom == a) return true;
            if (idom == idom->idom) break; // 防止循环
            idom = idom->idom;
        }
        return false;
    }

    // 为循环创建预头块
    void create_preheader(IRFunction &F, LoopInfo *loop) {
        IRBasicBlock *header = loop->header;

        // 检查是否已经有预头块
        if (header->predecessors.size() == 1) {
            IRBasicBlock *pred = header->predecessors[0];
            if (!loop->blocks.count(pred) && pred->successors.size() == 1) {
                loop->preheader = pred;
                return;
            }
        }

        // 创建新的预头块
        auto preheader_ptr = std::make_unique<IRBasicBlock>("preheader" + header->label);
        IRBasicBlock *preheader = preheader_ptr.get();
        loop->preheader = preheader;

        // 添加 LABEL 指令
        preheader->insts.emplace_back(IROp::LABEL, std::vector<IROperand>{
                                                       IROperand::create_label(preheader->label) });

        // 添加跳转到循环头的指令
        preheader->insts.emplace_back(IROp::BR, std::vector<IROperand>{
                                                    IROperand::create_label(header->label) });

        // 更新 CFG：将所有从循环外进入循环头的边重定向到预头块
        std::vector<IRBasicBlock *> external_preds;
        for (auto pred : header->predecessors) {
            if (!loop->blocks.count(pred)) {
                external_preds.push_back(pred);
            }
        }

        // 更新前驱的后继
        for (auto pred : external_preds) {
            // 替换后继中的 header 为 preheader
            for (size_t i = 0; i < pred->successors.size(); ++i) {
                if (pred->successors[i] == header) {
                    pred->successors[i] = preheader;
                }
            }

            // 更新跳转指令中的标签
            for (auto &inst : pred->insts) {
                if (inst.op == IROp::BR || inst.op == IROp::BRZ || inst.op == IROp::BRLT ||
                    inst.op == IROp::BRGT) {
                    for (auto &arg : inst.args) {
                        if (arg.op_type == IROperandType::LABEL && arg.name == header->label) {
                            arg.name = preheader->label;
                        }
                    }
                }
            }

            preheader->predecessors.push_back(pred);
        }

        // 更新循环头的前驱
        for (auto pred : external_preds) {
            header->predecessors.erase(std::remove(header->predecessors.begin(),
                                                   header->predecessors.end(), pred),
                                       header->predecessors.end());
        }
        header->predecessors.push_back(preheader);
        preheader->successors.push_back(header);

        // 将预头块插入到函数中（在循环头之前）
        auto it = std::find_if(F.blocks.begin(), F.blocks.end(), [header](const auto &b) {
            return b.get() == header;
        });
        F.blocks.insert(it, std::move(preheader_ptr));
    }

    // 检查指令是否是循环不变式
    bool is_loop_invariant(IRInstruction *inst, LoopInfo *loop,
                           std::unordered_set<IRInstruction *> &invariants) {
        // 不能外提的指令类型（有副作用或特殊用途的指令）
        if (inst->op == IROp::LOAD || inst->op == IROp::STORE || inst->op == IROp::CALL ||
            inst->op == IROp::ALLOCA || inst->op == IROp::PHI || inst->op == IROp::LABEL ||
            inst->op == IROp::MOVE || inst->is_terminator() ||
            // I/O指令有副作用，不能外提
            inst->op == IROp::INPUT_I32 || inst->op == IROp::INPUT_I8 ||
            inst->op == IROp::OUTPUT_I32 || inst->op == IROp::OUTPUT_I8 ||
            inst->op == IROp::OUTPUT_STR) {
            return false;
        }

        // 检查所有操作数
        for (const auto &arg : inst->args) {
            if (arg.op_type == IROperandType::REG) {
                // 查找定义该寄存器的指令
                auto def_it = current_function->var_def_inst_map.find(arg.name);
                if (def_it != current_function->var_def_inst_map.end()) {
                    IRInstruction *def_inst = def_it->second;
                    IRBasicBlock *def_block = current_function->inst_to_block_map[def_inst];

                    // 如果定义在循环内
                    if (loop->blocks.count(def_block)) {
                        // 必须已经被标记为不变式
                        if (!invariants.count(def_inst)) {
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    // 检查指令是否可以安全地移动
    bool is_safe_to_move(IRInstruction *inst, LoopInfo *loop) {
        IRBasicBlock *inst_block = current_function->inst_to_block_map[inst];

        // 如果循环没有退出块（无限循环），则不安全
        if (loop->exit_blocks.empty()) {
            return false;
        }

        // 检查该指令的所有使用
        auto use_it = current_function->def_use_chain.find(inst);
        if (use_it != current_function->def_use_chain.end()) {
            bool has_use_outside_loop = false;

            for (auto use_inst : use_it->second) {
                IRBasicBlock *use_block = current_function->inst_to_block_map[use_inst];

                // 如果有使用在循环外
                if (!loop->blocks.count(use_block)) {
                    has_use_outside_loop = true;
                    break;
                }
            }

            // 如果有循环外的使用，必须支配所有退出块
            if (has_use_outside_loop) {
                for (auto exit : loop->exit_blocks) {
                    if (!dominates(inst_block, exit)) {
                        return false;
                    }
                }
            }
        }

        // 不产生副作用的指令都可以外提（即使可能不执行）
        // 因为它们只是计算，不会改变程序状态
        return true;
    }

    // 对单个循环执行 LICM
    bool hoist_loop_invariants(IRFunction &F, LoopInfo *loop) {
        bool changed = false;

        // 迭代查找循环不变式
        std::unordered_set<IRInstruction *> invariants;
        bool found_new = true;

        while (found_new) {
            found_new = false;

            for (auto block : loop->blocks) {
                for (auto &inst : block->insts) {
                    if (invariants.count(&inst)) continue;

                    if (is_loop_invariant(&inst, loop, invariants)) {
                        invariants.insert(&inst);
                        found_new = true;
                    }
                }
            }
        }

        // 外提循环不变式
        std::vector<std::pair<IRBasicBlock *, IRInstruction *>> to_hoist;

        for (auto inv_inst : invariants) {
            IRBasicBlock *block = current_function->inst_to_block_map[inv_inst];

            // 检查是否安全移动
            if (is_safe_to_move(inv_inst, loop)) {
                to_hoist.push_back({ block, inv_inst });
            }
        }

        // 如果没有需要外提的指令，直接返回
        if (to_hoist.empty()) {
            return false;
        }

        // 确保有预头块（只在需要时创建）
        if (!loop->preheader) {
            create_preheader(F, loop);
        }

        // 执行外提
        for (auto [block, inst] : to_hoist) {
            // 从原块中移除指令
            auto it = std::find_if(block->insts.begin(), block->insts.end(),
                                   [inst](const IRInstruction &i) { return &i == inst; });
            if (it != block->insts.end()) {
                IRInstruction moved_inst = std::move(*it);
                block->insts.erase(it);

                // 插入到预头块的跳转指令之前
                auto preheader_it = loop->preheader->insts.end();
                --preheader_it; // 跳过最后的 BR 指令
                loop->preheader->insts.insert(preheader_it, std::move(moved_inst));

                changed = true;

                std::cout << "    Hoisted invariant from " << block->label << std::endl;
            }
        }

        return changed;
    }

  public:
    bool run(IRFunction &F) override {
        std::cout << "Running LICMPass on function: " << F.name << std::endl;

        current_function = &F;
        bool changed = false;

        // 检测循环
        detect_loops(F);

        std::cout << "  Found " << all_loops.size() << " loop(s)" << std::endl;

        // 对每个循环执行 LICM
        for (auto &loop : all_loops) {
            std::cout << "  Processing loop with header: " << loop->header->label << std::endl;
            if (hoist_loop_invariants(F, loop.get())) {
                changed = true;
            }
        }

        current_function = nullptr;
        return changed;
    }
};
