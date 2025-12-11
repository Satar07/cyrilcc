#pragma once

#include "ir.hpp"
#include "pass.hpp"
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// ========================================================
// --- GVN (Global Value Numbering) Pass ---
// ========================================================

class GVNPass : public FunctionPass {
  private:
    /**
     * @brief GVN 的核心 "值" 的唯一键。
     * 用于 valueTable 的 Key。
     */
    struct ValueKey {
        IROp op;
        std::vector<size_t> operand_vns; // 操作数的值编号

        // 用于常量或全局地址
        int imm = 0;
        std::string name;

        // 构造函数 (常量)
        ValueKey(int i) : op(IROp::MOVE), imm(i) {} // 使用 MOVE 作为常量的代理 Op
        // 构造函数 (全局地址)
        ValueKey(std::string n) : op(IROp::GEP), name(std::move(n)) {} // 使用 GEP 作为地址的代理 Op
        // 构造函数 (计算)
        ValueKey(IROp o, std::vector<size_t> vns) : op(o), operand_vns(std::move(vns)) {
            // 对可交换操作进行规范化 (ADD, MUL)
            if (op == IROp::ADD || op == IROp::MUL) {
                if (operand_vns.size() == 2 && operand_vns[0] > operand_vns[1]) {
                    std::swap(operand_vns[0], operand_vns[1]);
                }
            }
        }

        bool operator==(const ValueKey &other) const {
            return op == other.op && imm == other.imm && name == other.name &&
                   operand_vns == other.operand_vns;
        }
    };

    /**
     * @brief ValueKey 的哈希函数
     */
    struct ValueKeyHash {
        std::size_t operator()(const ValueKey &k) const {
            size_t hash = std::hash<int>()(static_cast<int>(k.op));
            hash ^= std::hash<int>()(k.imm) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<std::string>()(k.name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            for (size_t vn : k.operand_vns) {
                hash ^= std::hash<size_t>()(vn) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    // --- GVN 核心数据结构 ---

    // 1. 值 -> 编号
    // (e.g., (ADD, VN_5, VN_6) -> VN_7)
    std::unordered_map<ValueKey, size_t, ValueKeyHash> valueTable;

    // 2. 虚拟寄存器 -> 编号
    // (e.g., "%1" -> VN_7)
    std::unordered_map<std::string, size_t> regToVN;

    // 3. 编号 -> 规范操作数 (第一个计算出该值的操作数)
    // (e.g., VN_7 -> IROperand("%1"))
    std::unordered_map<size_t, IROperand> vnToReg;

    // 计数器
    size_t nextVN = 1;
    bool ir_changed = false;

    /**
     * @brief 获取一个操作数的值编号 (VN)
     */
    size_t getVN(const IROperand &op) {
        switch (op.op_type) {
            case IROperandType::IMM: {
                ValueKey key(op.imm_value);
                if (valueTable.count(key)) return valueTable.at(key);
                size_t vn = nextVN++;
                valueTable[key] = vn;
                vnToReg[vn] = op;
                return vn;
            }
            case IROperandType::GLOBAL: {
                ValueKey key(op.name);
                if (valueTable.count(key)) return valueTable.at(key);
                size_t vn = nextVN++;
                valueTable[key] = vn;
                vnToReg[vn] = op;
                return vn;
            }
            case IROperandType::REG: {
                // 寄存器必须在 regToVN 中有定义
                // (因为我们按支配树顺序遍历，定义总是在使用之前)
                if (regToVN.count(op.name)) {
                    return regToVN.at(op.name);
                }
                // Fallback: 如果来自参数，现场分配一个
                size_t vn = nextVN++;
                regToVN[op.name] = vn;
                vnToReg[vn] = op;
                return vn;
            }
            default:
                // LABEL 等
                return 0; // "未知" VN
        }
    }

    /**
     * @brief 递归处理基本块 (按支配树前序遍历)
     */
    void processBlock(IRBasicBlock *block) {
        // 用于作用域哈希：记录在此块中添加的条目，以便在返回时撤销
        std::vector<std::string> regsDefinedInBlock;
        std::vector<ValueKey> valuesDefinedInBlock;

        for (auto &inst : block->insts) {
            // GVN 复制传播
            // 处理 `%y = move %x`
            if (inst.op == IROp::MOVE) {
                if (inst.result.has_value() && inst.args.size() == 1) {
                    size_t vn = getVN(inst.args[0]);
                    std::string regName = inst.result->name;
                    regToVN[regName] = vn;
                    regsDefinedInBlock.push_back(regName);
                }
                continue; // 处理下一条指令
            }

            // GVN 冗余计算消除
            // 检查是否是可 GVN 的计算 (ADD, SUB, MUL, DIV, GEP)
            if (inst.is_calc() || inst.op == IROp::GEP) {
                // 获取所有操作数的值编号
                std::vector<size_t> vns;
                for (const auto &arg : inst.args) {
                    vns.push_back(getVN(arg));
                }
                // 构建 ValueKey
                ValueKey key(inst.op, vns);

                // 查找
                if (valueTable.count(key)) {
                    size_t existingVN = valueTable.at(key);
                    IROperand canonicalReg = vnToReg.at(existingVN); // 规范操作数
                    std::string oldRegName = inst.result->name;

                    std::cout << "GVN: Replacing " << oldRegName
                              << " (Op: " << op_to_string(inst.op) << ") with "
                              << canonicalReg.to_string() << std::endl;

                    // 替换：将 `inst` 转换为 `move`
                    inst.op = IROp::MOVE;
                    inst.args = { canonicalReg };

                    // 更新映射
                    regToVN[oldRegName] = existingVN;
                    regsDefinedInBlock.push_back(oldRegName);
                    ir_changed = true;
                } else {
                    // 这是一个新值
                    size_t newVN = nextVN++;
                    std::string regName = inst.result->name;

                    // 添加到所有映射
                    valueTable[key] = newVN;
                    regToVN[regName] = newVN;
                    vnToReg[newVN] = *inst.result; // 这是此 VN 的规范操作数

                    // 记录以便撤销
                    regsDefinedInBlock.push_back(regName);
                    valuesDefinedInBlock.push_back(key);
                }
            } else if (inst.result.has_value() && inst.result->op_type == IROperandType::REG) {
                size_t newVN = nextVN++;
                std::string regName = inst.result->name;
                regToVN[regName] = newVN;
                vnToReg[newVN] = *inst.result;
                regsDefinedInBlock.push_back(regName);
            }
        } // 遍历块中的指令结束

        // --- 4. 递归支配树 ---
        // 访问所有此块“直接支配”的子块
        for (IRBasicBlock *child : block->dom_child) {
            processBlock(child);
        }

        // --- 5. 撤销 (Pop Scope) ---
        // 当我们从此块返回时 (已处理完所有子树)，
        // 撤销此块中添加的定义，以便访问兄弟块。
        for (const auto &key : valuesDefinedInBlock) {
            valueTable.erase(key);
        }
        for (const auto &regName : regsDefinedInBlock) {
            size_t vn = regToVN[regName];
            // 只有当此寄存器是该VN的规范寄存器时，才从 vnToReg 中删除
            if (vnToReg.count(vn) && vnToReg[vn].name == regName) {
                vnToReg.erase(vn);
            }
            regToVN.erase(regName);
        }
    }

  public:
    bool run(IRFunction &F) override {
        std::cout << "Running GVNPass on function: " << F.name << std::endl;

        // 确保CFG和支配树已构建
        if (F.blocks.empty() || F.blocks[0]->idom != nullptr) {
            if (F.blocks.empty()) return false;
            std::cerr << "GVNPass: Dominator tree seems not built correctly (entry block has idom)."
                      << std::endl;
            // 尽管如此，我们还是假设 F.blocks[0] 是入口
        }

        // 1. 清理状态
        valueTable.clear();
        regToVN.clear();
        vnToReg.clear();
        nextVN = 1;
        ir_changed = false;

        // 2. 为函数参数预先分配 VN
        for (const auto &param : F.params) {
            size_t vn = nextVN++;
            regToVN[param.name] = vn;
            vnToReg[vn] = param;
        }

        // 3. 从入口块开始递归
        if (!F.blocks.empty()) {
            processBlock(F.blocks[0].get());
        }

        return ir_changed;
    }
};