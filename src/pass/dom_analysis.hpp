#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class DataFlowAnalysisPass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::cout << "Running DataFlowAnalysisPass on function: " << F.name << std::endl;

        F.label_to_block_map.clear();
        F.inst_to_block_map.clear();
        F.var_def_inst_map.clear();
        F.def_use_chain.clear();

        for (auto &block : F.blocks) {
            F.label_to_block_map[block->label] = block.get();
            for (auto &inst : block->insts) {
                F.inst_to_block_map[&inst] = block.get();
                // 定义
                if (inst.result.has_value() and inst.result->op_type == IROperandType::REG) {
                    F.var_def_inst_map.insert({ inst.result->name, &inst });
                    F.def_use_chain.insert({ &inst, {} });
                }
            }
        }

        // 使用
        for (auto &block : F.blocks) {
            for (auto &inst : block->insts) {
                for (const auto &arg : inst.args) {
                    if (arg.op_type == IROperandType::REG) {
                        if (F.var_def_inst_map.contains(arg.name)) {
                            IRInstruction *def_inst = F.var_def_inst_map.at(arg.name);
                            F.def_use_chain[def_inst].push_back(&inst);
                        }
                    }
                }
            }
        }

        return false;
    }
};

// --- CFG 构建 Pass ---
class BuildCFGPass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::cout << "Running BuildCFGPass on function: " << F.name << std::endl;
        std::unordered_map<std::string, IRBasicBlock *> label_map;
        for (auto &block : F.blocks) {
            label_map[block->label] = block.get();
            block->successors.clear();
            block->predecessors.clear();
        }

        // 加边
        auto add_edge = [&](IRBasicBlock *from, IRBasicBlock *to) {
            if (!from || !to) return; // 安全检查
            // 避免重复添加后继
            if (std::find(from->successors.begin(), from->successors.end(), to) ==
                from->successors.end()) {
                from->successors.push_back(to);
            }
            // 避免重复添加前驱
            if (std::find(to->predecessors.begin(), to->predecessors.end(), from) ==
                to->predecessors.end()) {
                to->predecessors.push_back(from);
            }
        };

        // 遍历所有基本块
        for (size_t i = 0; i < F.blocks.size(); ++i) {
            auto &block = F.blocks[i];
            bool has_unconditional_terminator = false;

            for (const IRInstruction &inst : block->insts) {
                switch (inst.op) {
                    case IROp::RET: has_unconditional_terminator = true; break;
                    case IROp::BR: {
                        // 无条件跳转：添加一个后继
                        std::string target_label = inst.args[0].name;
                        if (label_map.count(target_label)) {
                            add_edge(block.get(), label_map[target_label]);
                        }
                        has_unconditional_terminator = true;
                        break;
                    }
                    case IROp::BRZ:
                    case IROp::BRLT:
                    case IROp::BRGT: {
                        // 条件跳转：添加一个后继
                        std::string target_label = inst.args[0].name;
                        if (label_map.count(target_label)) {
                            add_edge(block.get(), label_map[target_label]);
                        }
                        break;
                    }
                    default:
                        // 非终结指令
                        break;
                }
            }

            // 处理隐式“fall-through”
            if (!has_unconditional_terminator && (i + 1) < F.blocks.size()) {
                add_edge(block.get(), F.blocks[i + 1].get());
            }
        }
        return false;
    }
};

class DeadBlockEliminationPass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::cout << "Running DeadBlockEliminationPass on function: " << F.name << std::endl;
        bool ir_changed = false;
        if (F.blocks.empty()) return false;

        while (true) {
            std::unordered_set<IRBasicBlock *> dead_blocks;

            for (auto it = F.blocks.begin() + 1; it != F.blocks.end(); ++it) {
                if (it->get()->predecessors.empty()) {
                    dead_blocks.insert(it->get());
                }
            }

            if (dead_blocks.empty()) {
                break; // 没有找到死块，退出循环
            }

            ir_changed = true;

            for (auto &block : F.blocks) {
                block->predecessors.erase(
                    std::remove_if(block->predecessors.begin(), block->predecessors.end(),
                                   [&](IRBasicBlock *succ) { return dead_blocks.contains(succ); }),
                    block->predecessors.end());
            }

            F.blocks.erase(std::remove_if(F.blocks.begin() + 1, F.blocks.end(),
                                          [&](auto &block) {
                                              return dead_blocks.contains(block.get());
                                          }),
                           F.blocks.end());
        }

        return ir_changed;
    }
};

// --- 支配树分析 Pass ---
class DominatorTreePass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::cout << "Running DominatorTreePass on function: " << F.name << std::endl;
        auto &blocks = F.blocks;
        if (blocks.empty()) return false;

        auto entry = blocks.data()->get();
        const auto num_blocks = blocks.size();

        std::unordered_map<IRBasicBlock *, std::unordered_set<IRBasicBlock *>>
            dom_calc; // <d,{n}> d -> n
        std::unordered_set<IRBasicBlock *> all_nodes;

        for (auto &block : blocks) {
            all_nodes.insert(block.get());
        }
        dom_calc.insert({ entry, { entry } });
        auto it = blocks.begin();
        std::advance(it, 1);
        for (; it != blocks.end(); ++it) {
            dom_calc.insert({ it->get(), all_nodes });
        }

        bool changed = true;
        while (changed) {
            changed = false;
            // 跳过entry
            for (size_t i = 1; i < num_blocks; ++i) {
                auto &block = blocks[i];
                std::cout << "now calc block: " << block->label << std::endl;
                // {d} = dom N = intersection of dom P for all P in predecessors(N) + N
                std::unordered_set<IRBasicBlock *> new_dom;
                for (auto pred : block->predecessors) {
                    // if (not dom_calc.contains(pred)) continue;
                    if (new_dom.empty()) {
                        new_dom = dom_calc[pred];
                        std::cout << "now is empty, so add in set: ";
                        for (auto b : new_dom) {
                            std::cout << b->label << " ";
                        }
                        std::cout << std::endl;
                        continue;
                    }
                    std::unordered_set<IRBasicBlock *> temp;
                    for (auto b : new_dom) {
                        std::cout << "check block " << b->label << " in pred " << pred->label
                                  << std::endl;
                        if (dom_calc[pred].contains(b)) {
                            std::cout << "keep block " << b->label << std::endl;
                            temp.insert(b);
                        } else {
                            std::cout << "erase block " << b->label << std::endl;
                        }
                    }
                    new_dom = std::move(temp);
                }
                new_dom.insert(block.get());
                if (dom_calc.at(block.get()) != new_dom) {
                    dom_calc[block.get()] = std::move(new_dom);
                    changed = true;
                }
            }
        }

        // 计算 idom 和 dom_child
        for (size_t i = 1; i < num_blocks; ++i) {
            auto &block_n = blocks[i];
            // sdom(N) = dom(N) - {N}
            // 检查 d 是否是 N 的 "直接" 支配者
            // d 的所有支配者 m (sdom(d))，是否也在 N 的支配者 (sdom(N)) 中
            for (auto dom_d : dom_calc[block_n.get()]) {
                if (dom_d == block_n.get()) continue;
                bool is_idom = true;
                // 遍历 N 的其他支配节点 m (不包括 N 和 d)
                for (auto dom_m : dom_calc[block_n.get()]) {
                    if (dom_m == block_n.get() || dom_m == dom_d) continue;
                    if (dom_calc[dom_d].find(dom_m) == dom_calc[dom_d].end()) {
                        is_idom = false;
                        break;
                    }
                }
                if (is_idom) {
                    block_n.get()->idom = dom_d;
                    dom_d->dom_child.push_back(block_n.get());
                    break;
                }
            }
        }

        return false;
    }
};

// --- 支配边界分析 Pass ---
class DominanceFrontierPass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::cout << "Running DominanceFrontierPass on function: " << F.name << std::endl;
        auto &blocks = F.blocks;
        if (blocks.empty()) return false;

        // 辅助函数：检查 n 是否严格支配 w (n->w)
        auto strictly_dominates = [](IRBasicBlock *n, IRBasicBlock *w) {
            if (!w || !n) return false;
            IRBasicBlock *temp = w->idom;
            while (temp) {
                if (temp == n) {
                    return true;
                }
                if (temp == temp->idom) break;
                temp = temp->idom;
            }
            return false;
        };

        // 递归 lambda，用于自底向上遍历支配树
        std::function<void(IRBasicBlock *)> compute_df_recursive;
        compute_df_recursive = [&](IRBasicBlock *n) {
            // DF_local(n) = { s | s in succ(n) and idom(s) != n }
            // 计算由节点n的直接后继贡献的支配边界节点
            for (IRBasicBlock *s : n->successors) {
                if (s->idom != n) {
                    n->dom_frontiers.insert(s);
                }
            }
            // DF_up(n) = Union { w | w in DF(c) and n 不严格支配 w }
            //            for c in dom_children(n)
            // 从后继支配边界继承符合定义的支配边界
            for (IRBasicBlock *c : n->dom_child) {
                compute_df_recursive(c);
                for (IRBasicBlock *w : c->dom_frontiers) {
                    if (!strictly_dominates(n, w)) {
                        n->dom_frontiers.insert(w);
                    }
                }
            }
        };

        // 从入口块开始递归
        compute_df_recursive(blocks[0].get());

        return false; // 分析 Pass 不修改 IR
    }
};
