#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// --- CFG 构建 Pass ---
class BuildCFGPass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        std::unordered_map<std::string, IRBasicBlock *> label_map;
        for (IRBasicBlock &block : F.blocks) {
            label_map[block.label] = &block;
            block.successors.clear();
            block.predecessors.clear();
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
            IRBasicBlock &block = F.blocks[i];
            bool has_unconditional_terminator = false;

            for (const IRInstruction &inst : block.insts) {
                switch (inst.op) {
                    case IROp::RET: has_unconditional_terminator = true; break;
                    case IROp::BR: {
                        // 无条件跳转：添加一个后继
                        std::string target_label = inst.args[0].name;
                        if (label_map.count(target_label)) {
                            add_edge(&block, label_map[target_label]);
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
                            add_edge(&block, label_map[target_label]);
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
                add_edge(&block, &F.blocks[i + 1]);
            }
        }
        return false;
    }
};

// --- 支配树分析 Pass ---
class DominatorTreePass : public FunctionPass {
  public:
    bool run(IRFunction &F) override {
        auto &blocks = F.blocks;
        if (blocks.empty()) return false;

        IRBasicBlock *entry = blocks.data();
        const auto num_blocks = blocks.size();

        std::unordered_map<IRBasicBlock *, std::unordered_set<IRBasicBlock *>>
            dom_calc; // <d,{n}> d -> n
        std::unordered_set<IRBasicBlock *> all_nodes;

        for (auto &block : blocks) {
            all_nodes.insert(&block);
        }
        dom_calc.insert({ entry, { entry } });
        auto it = blocks.begin()++;
        for (; it != blocks.end(); ++it) {
            dom_calc.insert({ &(*it), all_nodes });
        }

        bool changed = true;
        while (changed) {
            changed = false;

            // 跳过entry
            for (size_t i = 1; i < num_blocks; ++i) {
                IRBasicBlock *block = &blocks[i];
                // dom N = intersection of dom P for all P in predecessors(N) + N
                std::unordered_set<IRBasicBlock *> new_dom;
                for (auto pred : block->predecessors) {
                    if (dom_calc.find(pred) == dom_calc.end()) continue;
                    if (new_dom.empty()) {
                        new_dom = dom_calc[pred];
                    } else {
                        std::unordered_set<IRBasicBlock *> temp;
                        for (auto b : new_dom) {
                            if (dom_calc[pred].count(b)) {
                                temp.insert(b);
                            }
                        }
                        new_dom = temp;
                    }
                }
                new_dom.insert(block);
                if (dom_calc.at(block) != new_dom) {
                    dom_calc[block] = std::move(new_dom);
                    changed = true;
                }
            }
        }

        // 计算 idom 和 dom_child
        for (size_t i = 1; i < num_blocks; ++i) {
            IRBasicBlock *block_n = &blocks[i];
            // sdom(N) = dom(N) - {N}
            // 检查 d 是否是 N 的 "直接" 支配者
            // d 的所有支配者 m (sdom(d))，是否也在 N 的支配者 (sdom(N)) 中
            for (auto dom_d : dom_calc[block_n]) {
                if (dom_d == block_n) continue;
                bool is_idom = true;
                // 遍历 N 的其他支配节点 m (不包括 N 和 d)
                for (auto dom_m : dom_calc[block_n]) {
                    if (dom_m == block_n || dom_m == dom_d) continue;
                    if (dom_calc[dom_d].find(dom_m) == dom_calc[dom_d].end()) {
                        is_idom = false;
                        break;
                    }
                }
                if (is_idom) {
                    block_n->idom = dom_d;
                    dom_d->dom_child.push_back(block_n);
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
        compute_df_recursive(&blocks[0]);

        return false; // 分析 Pass 不修改 IR
    }
};
