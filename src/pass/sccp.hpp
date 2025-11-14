#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include <cstddef>
#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

enum class LatticeStatus {
    UNKNOWN,  // 还不知道 T
    CONST,    // 真是常量 C
    NOT_CONST // 真不是常量 B
};

inline std::string to_string(LatticeStatus status) {
    switch (status) {
        case LatticeStatus::UNKNOWN: return "UNKNOWN";
        case LatticeStatus::CONST: return "CONST";
        case LatticeStatus::NOT_CONST: return "NOT_CONST";
    }
    return "INVALID";
}

struct LatticeValue {
    LatticeStatus status = LatticeStatus::UNKNOWN;
    int value = 0;

    LatticeValue(const LatticeStatus s, const int v = 0) : status(s), value(v) {}

    bool is_const() const {
        return status == LatticeStatus::CONST;
    }

    bool is_not_const() const {
        return status == LatticeStatus::NOT_CONST;
    }

    bool is_unknown() const {
        return status == LatticeStatus::UNKNOWN;
    }

    bool operator==(const LatticeValue &other) const {
        if (status != other.status) return false;
        if (status == LatticeStatus::CONST) {
            return value == other.value;
        }
        return true;
    }

    bool operator!=(const LatticeValue &other) const {
        return !(*this == other);
    }

    LatticeValue meet(const LatticeValue &other) const {
        if (this->is_not_const() || other.is_not_const()) {
            return { LatticeStatus::NOT_CONST };
        }
        if (this->is_unknown()) {
            return other;
        }
        if (other.is_unknown()) {
            return *this;
        }
        if (this->is_const() && other.is_const()) {
            if (this->value == other.value) {
                return *this;
            }
        }
        return { LatticeStatus::NOT_CONST };
    }
};

class SCCPPass : public FunctionPass {
  private:
    std::unordered_map<std::string, LatticeValue> ssa_value_map;
    std::unordered_set<IRBasicBlock *> executable_blocks;

    std::deque<IRBasicBlock *> block_worklist;
    std::deque<IRInstruction *> ssa_worklist;

    IRFunction *current_function = nullptr;

    bool ir_changed = false;

    // 获取操作数的格值
    LatticeValue get_operand_value(const IROperand &op) const {
        if (op.op_type == IROperandType::IMM) {
            return { LatticeStatus::CONST, op.imm_value };
        }
        if (op.op_type == IROperandType::REG) {
            if (ssa_value_map.contains(op.name)) {
                return ssa_value_map.at(op.name);
            }
            return { LatticeStatus::UNKNOWN };
        }
        // 全局变量等
        return { LatticeStatus::NOT_CONST };
    }

    // 设置SSA寄存器的格值，如果值改变就更新工作列表
    void set_value(IRInstruction *inst, LatticeValue new_val) {
        if (not inst->result) return;
        const auto &reg_name{ inst->result->name };
        if (ssa_value_map.contains(reg_name) and ssa_value_map.at(reg_name) == new_val) return;

        // 发生变化
        ssa_value_map.insert_or_assign(reg_name, new_val);
        ir_changed = true;
        if (not current_function->def_use_chain.contains(inst)) return;
        for (auto user : current_function->def_use_chain.at(inst)) {
            auto user_block = current_function->inst_to_block_map.at(user);
            if (not executable_blocks.contains(user_block)) continue;
            if (user->op == IROp::TEST or user->is_terminator()) {
                block_worklist.push_back(user_block);
                continue;
            }
            ssa_worklist.push_back(user);
        }
    }

    // 将块标记为可执行，并处理phi节点
    void mark_block_executable(IRBasicBlock *block) {
        if (block == nullptr) return;
        if (executable_blocks.contains(block)) return;
        executable_blocks.insert(block);
        block_worklist.push_back(block);
        ir_changed = true;
        for (auto &inst : block->insts) {
            if (inst.op == IROp::LABEL) continue;
            if (inst.op != IROp::PHI) break;
            ssa_worklist.push_back(&inst);
        }
        for (auto *succ : block->successors) {
            for (auto &inst : succ->insts) {
                if (inst.op == IROp::LABEL) continue;
                if (inst.op != IROp::PHI) break;
                ssa_worklist.push_back(&inst);
            }
        }
    }

    // 看看一条指令是怎么回事
    void visit_inst(IRInstruction *inst) {
        // phi指令
        if (inst->op == IROp::PHI) {
            std::cout << "Visiting PHI: " << inst->result->name << std::endl;
            LatticeValue phi_val{ LatticeStatus::UNKNOWN };
            for (size_t i = 0; i < inst->args.size(); i += 2) {
                const auto &ir_operand = inst->args.at(i);
                const auto &operand = inst->args.at(i + 1);
                auto pred_block = current_function->label_to_block_map.at(operand.name);
                std::cout << "  - Predecessor: " << operand.name << ", Executable: "
                          << executable_blocks.contains(pred_block) << std::endl;
                if (executable_blocks.contains(pred_block)) {
                    auto val = get_operand_value(ir_operand);
                    std::cout << "    - Value: " << to_string(val.status) << ", " << val.value
                              << std::endl;
                    phi_val = phi_val.meet(val);
                }
            }
            std::cout << "  - Result: " << to_string(phi_val.status) << ", " << phi_val.value
                      << std::endl;
            set_value(inst, phi_val);
            return;
        }

        if (inst->is_calc()) {
            const auto lhs = get_operand_value(inst->args.at(0));
            const auto rhs = get_operand_value(inst->args.at(1));
            if (lhs.is_unknown() or rhs.is_unknown()) {
                set_value(inst, { LatticeStatus::UNKNOWN });
                return;
            }
            if (lhs.is_const() and rhs.is_const()) {
                int result = 0;
                switch (inst->op) {
                    case IROp::ADD: result = lhs.value + rhs.value; break;
                    case IROp::SUB: result = lhs.value - rhs.value; break;
                    case IROp::MUL: result = lhs.value * rhs.value; break;
                    case IROp::DIV:
                        if (rhs.value == 0) {
                            set_value(inst, { LatticeStatus::NOT_CONST });
                            return;
                        }
                        result = lhs.value / rhs.value;
                        break;
                    default: throw std::runtime_error("Unknown calc op in SCCP");
                }
                set_value(inst, { LatticeStatus::CONST, result });
                return;
            }
            set_value(inst, { LatticeStatus::NOT_CONST });
            return;
        }

        if (inst->op == IROp::MOVE) {
            set_value(inst, get_operand_value(inst->args.at(0)));
            return;
        }
        // CALL, LOAD, GEP, INPUT 均视为 TOP
        if (inst->result.has_value()) {
            set_value(inst, { LatticeStatus::NOT_CONST });
        }
    }

    // 根据当前的格值，标记后续可达的块
    void visit_terminator(IRBasicBlock *block) {
        if (block->successors.empty()) {
            return;
        }

        if (block->successors.size() == 1) {
            mark_block_executable(block->successors.front());
            return;
        }

        const IRInstruction *last_test = nullptr;

        // 遍历块内的指令，找到相关的 TEST 和分支
        for (auto &inst : block->insts) {
            if (inst.op == IROp::TEST) {
                last_test = &inst;
                continue;
            }
            if (inst.op == IROp::RET) {
                return; // 块结束了
            }

            if (inst.op == IROp::BR) {
                // 这是一个无条件 'br'，它通常跟在 'brgt' 等后面
                mark_block_executable(
                    current_function->label_to_block_map.at(inst.args.at(0).name));
                return; // 这个 'br' 之后的任何指令都是死代码
            }

            if (inst.is_cond_b()) {
                const auto branch_succ =
                    current_function->label_to_block_map.at(inst.args.at(0).name);

                if (last_test == nullptr) {
                    // 没有 TEST？IR 格式有问题。
                    // 悲观假设：分支可能被执行
                    mark_block_executable(branch_succ);
                    continue; // 继续检查块内的下一条指令
                }

                auto lhs = get_operand_value(last_test->args.at(0));
                auto rhs = get_operand_value(last_test->args.at(1));

                if (lhs.is_not_const() or rhs.is_not_const()) {
                    // 状态是 NOT_CONST 或 UNKNOWN，无法确定
                    // 悲观假设：分支可能被执行
                    mark_block_executable(branch_succ);
                    continue; // 继续检查下一条指令（比如 else 分支的 br）
                }

                if (lhs.is_const() and rhs.is_const()) {
                    // 黄金情况：两个都是常量，我们可以立即求值
                    bool cond_met = false;
                    const auto v1 = lhs.value, v2 = rhs.value;
                    if (inst.op == IROp::BRZ)
                        cond_met = (v1 == v2);
                    else if (inst.op == IROp::BRLT)
                        cond_met = (v1 < v2);
                    else if (inst.op == IROp::BRGT)
                        cond_met = (v1 > v2);

                    if (cond_met) {
                        mark_block_executable(branch_succ);
                        return;
                    } else {
                        continue;
                    }
                }
            }
        }
    }

    void init(IRFunction &F) {
        current_function = &F;
        ssa_value_map.clear();
        executable_blocks.clear();
        block_worklist.clear();
        ssa_worklist.clear();
        ir_changed = false;

        for (auto &para : current_function->params) {
            ssa_value_map.insert({ para.name, { LatticeStatus::NOT_CONST } });
        }
    }

    void transform_ir() {
        if (!ir_changed) return; // 分析阶段说没啥可做的，直接退出

        // --- 这些列表现在只在转换阶段被填充 ---
        std::unordered_set<IRInstruction *> inst_to_delete;
        std::vector<std::pair<IRInstruction *, IROp>> branch_inst_to_change;
        std::vector<std::pair<IRInstruction *, LatticeValue>> const_inst_to_replace;

        // 遍历所有块
        for (auto &block_ptr : current_function->blocks) {
            IRBasicBlock *block = block_ptr.get();

            if (!executable_blocks.contains(block)) {
                // 这个块是死的，删除里面所有指令
                for (auto &inst : block->insts) {
                    if (inst.op != IROp::LABEL) {
                        inst_to_delete.insert(&inst);
                    }
                }
            } else {
                // 块是活的，检查里面的指令
                bool terminator_folded = false;
                const IRInstruction *last_test = nullptr;

                for (auto &inst : block->insts) {
                    if (terminator_folded) {
                        inst_to_delete.insert(&inst);
                        continue;
                    }

                    // 查找常量替换
                    if (inst.result && ssa_value_map.contains(inst.result->name)) {
                        if (LatticeValue val = ssa_value_map.at(inst.result->name);
                            val.is_const()) {
                            // 标记这条指令替换为 'move const'
                            const_inst_to_replace.emplace_back(&inst, val);
                        }
                    }

                    // 查找分支折叠
                    if (inst.op == IROp::TEST) {
                        last_test = &inst;
                        continue;
                    }
                    if (inst.op == IROp::BR || inst.op == IROp::RET) {
                        terminator_folded = true; // 块的末尾
                        continue;
                    }

                    if (inst.is_cond_b()) {
                        if (last_test == nullptr) continue; // 格式错误, 跳过

                        auto lhs = get_operand_value(last_test->args.at(0));
                        auto rhs = get_operand_value(last_test->args.at(1));

                        if (lhs.is_const() && rhs.is_const()) {
                            // 两个操作数都是常量，我们可以折叠这个分支
                            bool cond_met = false;
                            const auto v1 = lhs.value, v2 = rhs.value;
                            if (inst.op == IROp::BRZ)
                                cond_met = (v1 == v2);
                            else if (inst.op == IROp::BRLT)
                                cond_met = (v1 < v2);
                            else if (inst.op == IROp::BRGT)
                                cond_met = (v1 > v2);

                            if (cond_met) {
                                // 条件为真，把 'brgt' 变成 'br'
                                branch_inst_to_change.emplace_back(&inst, IROp::BR);
                                terminator_folded = true; // 后面的指令都死了
                            } else {
                                // 条件为假，这个 'brgt' 永远不执行
                                inst_to_delete.insert(&inst);
                                // 不要设置 terminator_folded，因为下一条指令(else的br)是活的
                            }
                        }
                    }
                }
            }
        }

        // 替换常量指令
        for (const auto &[inst, val] : const_inst_to_replace) {
            if (inst_to_delete.count(inst)) continue; // 别替换一个要被删除的指令
            IROperand imm = IROperand::create_imm(val.value, inst->result->type);
            inst->op = IROp::MOVE;
            inst->args = { imm };
        }

        // 转换分支
        for (const auto &[inst, new_op] : branch_inst_to_change) {
            if (inst_to_delete.count(inst)) continue;
            inst->op = new_op;
            inst->args = { inst->args[0] }; // 只保留 label
        }

        // 执行删除
        if (!inst_to_delete.empty()) {
            for (const auto &block : current_function->blocks) {
                block->insts.remove_if([&](IRInstruction &inst) {
                    return inst_to_delete.count(&inst);
                });
            }
        }
    }

  public:
    bool run(IRFunction &F) override {
        std::cout << "Running SCCP on function: " << F.name << std::endl;

        if (F.blocks.empty()) return false;

        init(F);

        mark_block_executable(current_function->blocks[0].get());

        while (!block_worklist.empty() || !ssa_worklist.empty()) {
            while (!block_worklist.empty()) {
                IRBasicBlock *block = block_worklist.front();
                block_worklist.pop_front();
                std::cout << "Visiting block: " << block->label << std::endl;

                for (auto &inst : block->insts) {
                    if (inst.is_terminator() or inst.op == IROp::TEST) break;
                    visit_inst(&inst);
                }

                visit_terminator(block);
            }

            while (!ssa_worklist.empty()) {
                IRInstruction *inst = ssa_worklist.front();
                ssa_worklist.pop_front();

                // 确保我们只评估可达块中的指令
                if (executable_blocks.contains(current_function->inst_to_block_map.at(inst))) {
                    visit_inst(inst);
                }
            }
        }

        transform_ir();

        return ir_changed;
    }
};
