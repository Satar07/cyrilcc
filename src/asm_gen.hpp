#pragma once

#include "ir.hpp" // 包含更新后的 ir.hpp
#include <array>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

// --- ABI 寄存器约定 ---
const int REG_FLAG = 0; // 标志
const int REG_IP = 1;   // 指令指针

const int REG_RETVAL = 2; // R2: 返回值 (v0) / 参数 (a0)
const int REG_ARG1 = 3;   // R3: 参数 (a1)
const int REG_ARG2 = 4;   // R4: 参数 (a2)
const int REG_ARG3 = 5;   // R5: 参数 (a3)
const int MAX_REGS_FOR_PARAMS = 4;

// 临时/暂存 (Caller-saved)
const int REG_T0 = 8;  // R8
const int REG_T1 = 9;  // R9
const int REG_T2 = 10; // R10
const int REG_T3 = 13; // R13

// 栈管理 (Callee-saved)
const int REG_FP = 11; // R11: 帧指针
const int REG_SP = 12; // R12: 栈指针

const int REG_RA = 14; // R14: 返回地址 (Special)
const int REG_IO = 15; // R15: I/O

const std::array<int, 4> SCRATCH_REGS = { 8, 9, 10, 13 };

class AsmGenerator {
  public:
    AsmGenerator(IRModule &mod, std::ostream &out) : module(mod), os(out) {};

    void generate() {
        // 1. 生成符号表
        gen_symbol();

        // 2. 生成代码段
        os << std::endl << "# --- Text Segment ---" << std::endl;
        emit("LOD R" + std::to_string(REG_SP) + ", 65535", "Init Stack Pointer");
        emit("LOD R" + std::to_string(REG_FP) + ", R" + std::to_string(REG_SP),
             "Init Frame Pointer");
        emit("LOD R" + std::to_string(REG_RA) + ", EXIT", "main func ret point");
        emit("JMP FUNCmain", "Jump to main function");
        emit_label("EXIT");
        emit("END");

        // 3. 遍历所有函数
        for (auto &func : module.functions) {
            visit_function(func);
        }

        // 4. 数据段
        visit_globals();
    }

  private:
    IRModule &module;
    std::ostream &os;

    // --- 状态量 ---
    std::unordered_map<std::string, std::string>
        global_label_map;                               // IR全局名 (@g) -> Asm标签 (VARg)
    std::unordered_map<std::string, int> alloca_map;    // 局部Alloca (%1) -> 栈偏移 (-4)
    std::unordered_map<std::string, int> temp_home_map; // 局部临时/参数 (%0) -> 栈偏移 (-8)
    std::unordered_map<std::string, int> reg_cache;     // 临时变量名 (%1) -> 物理寄存器 (R8)
    std::unordered_map<int, std::string> reg_cache_rev; // 物理寄存器 (R8) -> 临时变量名 (%1)

    int current_frame_size = 0;
    int label_counter = 0;

    // --- visitor ---
    void visit_globals() {
        os << std::endl << "# --- Data Segment ---" << std::endl;
        for (const auto &global : this->module.globals) {
            auto &name = global.name;
            auto asm_label = this->global_label_map.at(name);
            emit_label(asm_label);

            // 检查是否为字符串字面量
            if (global.type->is_pointer() && global.type->get_pointee_type()->is_char() &&
                !global.init_str.empty()) {
                std::string dbs = "DBS ";
                for (char c : global.init_str) {
                    dbs += std::to_string(static_cast<int>(c)) + ", ";
                }
                dbs += "0"; // null terminator
                emit(dbs, "String: " + global.escaped_init_str());
            } else {
                // 假定所有其他全局变量为 4 字节，零初始化
                emit("DBN 0, 4", "Global var: " + global.name);
            }
        }
    }

    void visit_function(const IRFunction &func) {
        const auto func_name = func.name;
        os << std::endl << "# --- Function: " << func_name << " ---" << std::endl;
        emit_label(global_label_map.at(func_name));

        // 清理状态
        this->alloca_map.clear();
        this->temp_home_map.clear();
        this->reg_cache.clear();
        this->reg_cache_rev.clear();

        auto local_stack_size = 0;
        auto param_stack_offset = 12; // FP + 8 (Old FP) + 4 (RA) = 12

        // 映射参数主页
        for (size_t i = 0; i < func.params.size(); ++i) {
            const auto &param_name = func.params.at(i).name;
            if (i < MAX_REGS_FOR_PARAMS) {
                // 寄存器传递的参数，在栈上为其分配 "主页"
                local_stack_size += 4;
                this->temp_home_map.insert({ param_name, -local_stack_size });
            } else {
                // 栈传递的参数，其 "主页" 就是它在调用者栈帧中的位置
                this->temp_home_map.insert({ param_name, param_stack_offset });
                param_stack_offset += 4;
            }
        }

        // 映射alloca和临时变量的主页
        for (const auto &block : func.blocks) {
            for (const auto &inst : block.insts) {
                if (inst.op == IROp::ALLOCA) {
                    auto name = inst.result.value().name;
                    local_stack_size += 4; // 假设所有 alloca 都是 4 字节
                    this->alloca_map.insert({ name, -local_stack_size });
                    continue;
                }
                // 如果指令有结果（不是alloca），则为临时变量
                if (inst.result && !inst.result->type->is_void()) {
                    auto name = inst.result.value().name;
                    local_stack_size += 4;
                    this->temp_home_map.insert({ name, -local_stack_size });
                }
            }
        }

        this->current_frame_size = local_stack_size;

        // 函数序言
        emit("STO (R" + std::to_string(REG_SP) + "), R" + std::to_string(REG_FP), "Push old FP");
        emit("SUB R" + std::to_string(REG_SP) + ", 4");
        emit("STO (R" + std::to_string(REG_SP) + "), R" + std::to_string(REG_RA),
             "Push return address (RA)");
        emit("SUB R" + std::to_string(REG_SP) + ", 4");
        emit("LOD R" + std::to_string(REG_FP) + ", R" + std::to_string(REG_SP), "FP = new SP");

        if (this->current_frame_size > 0) {
            emit("SUB R" + std::to_string(REG_SP) + ", " + std::to_string(this->current_frame_size),
                 "Allocate stack frame");
        }

        // 将寄存器中的参数存入其 "主页"
        for (size_t i = 0; i < func.params.size() && i < MAX_REGS_FOR_PARAMS; ++i) {
            const auto &param_name = func.params[i].name;
            int param_reg = REG_RETVAL + i;
            int offset = temp_home_map.at(param_name);
            emit("STO (R" + std::to_string(REG_FP) + format_offset(offset) + "), R" +
                     std::to_string(param_reg),
                 "Store param " + param_name + " to home");
        }

        // 访问指令
        for (const auto &block : func.blocks) {
            for (const auto &inst : block.insts) {
                visit_instruction(inst);
            }
        }
    }

    void visit_instruction(const IRInstruction &inst) {
        switch (inst.op) {
            case IROp::LABEL:
                spill_all_live_regs("Label");
                emit_label(get_asm_label(inst.args[0]));
                break;

            case IROp::RET: {
                if (!inst.args.empty()) {
                    ensure_in_reg(inst.args[0], REG_RETVAL);
                }
                emit("LOD R" + std::to_string(REG_SP) + ", R" + std::to_string(REG_FP),
                     "Restore SP");
                emit("LOD R" + std::to_string(REG_RA) + ", (R" + std::to_string(REG_SP) + " + 4)",
                     "Pop RA");
                emit("LOD R" + std::to_string(REG_FP) + ", (R" + std::to_string(REG_SP) + " + 8)",
                     "Pop old FP");
                emit("ADD R" + std::to_string(REG_SP) + ", 8", "Cleanup stack");
                emit("JMP R" + std::to_string(REG_RA), "Return");
                break;
            }

            case IROp::BR:
                spill_all_live_regs("BR");
                emit("JMP " + get_asm_label(inst.args[0]));
                break;

            case IROp::TEST:
                ensure_in_reg(inst.args[0], SCRATCH_REGS[0]);
                ensure_in_reg(inst.args[1], SCRATCH_REGS[1]);
                emit("SUB R" + std::to_string(SCRATCH_REGS[0]) + ", R" +
                         std::to_string(SCRATCH_REGS[1]),
                     "L - R");
                emit("TST R" + std::to_string(SCRATCH_REGS[0]));
                break;

            case IROp::BRZ:
                spill_all_live_regs("BRZ");
                emit("JEZ " + get_asm_label(inst.args[0]));
                break;
            case IROp::BRLT:
                spill_all_live_regs("BRLT");
                emit("JLZ " + get_asm_label(inst.args[0]));
                break;
            case IROp::BRGT:
                spill_all_live_regs("BRGT");
                emit("JGZ " + get_asm_label(inst.args[0]));
                break;

            case IROp::ALLOCA:
                // 已在 visit_function 中处理
                break;

            case IROp::LOAD: {
                std::string src_name = inst.args[0].name;
                std::string op_mnemonic = get_mem_op_mnemonic(inst.args[0], true);

                assign_to_reg(inst.result.value(), SCRATCH_REGS[0]);

                if (alloca_map.count(src_name)) {
                    int src_offset = alloca_map.at(src_name);
                    emit(op_mnemonic + " R" + std::to_string(SCRATCH_REGS[0]) + ", (R" +
                             std::to_string(REG_FP) + format_offset(src_offset) + ")",
                         "Load from alloca");
                } else if (global_label_map.count(src_name)) {
                    ensure_in_reg(inst.args[0], SCRATCH_REGS[1]);
                    emit(op_mnemonic + " R" + std::to_string(SCRATCH_REGS[0]) + ", (R" +
                             std::to_string(SCRATCH_REGS[1]) + ")",
                         "Load from global var");
                } else {
                    // 这是指针解引用 (LOAD %2, %1)
                    ensure_in_reg(inst.args[0], SCRATCH_REGS[1]);
                    emit(op_mnemonic + " R" + std::to_string(SCRATCH_REGS[0]) + ", (R" +
                             std::to_string(SCRATCH_REGS[1]) + ")",
                         "Load from pointer");
                }
                break;
            }

            case IROp::STORE: {
                ensure_in_reg(inst.args[0], SCRATCH_REGS[0]);
                std::string op_mnemonic = get_mem_op_mnemonic(inst.args[1], false);
                std::string dest_name = inst.args[1].name;

                if (alloca_map.count(dest_name)) {
                    int dest_offset = alloca_map.at(dest_name);
                    emit(op_mnemonic + " (R" + std::to_string(REG_FP) + format_offset(dest_offset) +
                             "), R" + std::to_string(SCRATCH_REGS[0]),
                         "Store to alloca");
                } else if (global_label_map.count(dest_name)) {
                    ensure_in_reg(inst.args[1], SCRATCH_REGS[1]);
                    emit(op_mnemonic + " (R" + std::to_string(SCRATCH_REGS[1]) + "), R" +
                             std::to_string(SCRATCH_REGS[0]),
                         "Store to global var");
                } else {
                    // 这是指针解引用 (STORE %val, %ptr)
                    ensure_in_reg(inst.args[1], SCRATCH_REGS[1]);
                    emit(op_mnemonic + " (R" + std::to_string(SCRATCH_REGS[1]) + "), R" +
                             std::to_string(SCRATCH_REGS[0]),
                         "Store to pointer");
                }
                break;
            }

            // 二元操作
            case IROp::ADD:
            case IROp::SUB:
            case IROp::MUL:
            case IROp::DIV: {
                std::string op_str;
                if (inst.op == IROp::ADD)
                    op_str = "ADD";
                else if (inst.op == IROp::SUB)
                    op_str = "SUB";
                else if (inst.op == IROp::MUL)
                    op_str = "MUL";
                else if (inst.op == IROp::DIV)
                    op_str = "DIV";
                else
                    op_str = "MOD";

                ensure_in_reg(inst.args[0], SCRATCH_REGS[0]);
                ensure_in_reg(inst.args[1], SCRATCH_REGS[1]);
                assign_to_reg(inst.result.value(), SCRATCH_REGS[2]);

                emit("LOD R" + std::to_string(SCRATCH_REGS[2]) + ", R" +
                         std::to_string(SCRATCH_REGS[0]),
                     "Move L to Dest");
                emit(op_str + " R" + std::to_string(SCRATCH_REGS[2]) + ", R" +
                         std::to_string(SCRATCH_REGS[1]),
                     "Binary op");
                break;
            }

            // 函数调用
            case IROp::CALL: {
                spill_all_live_regs("Call");
                int stack_arg_size = 0;

                for (size_t i = 1; i < inst.args.size(); ++i) {
                    if (i - 1 < MAX_REGS_FOR_PARAMS) {
                        ensure_in_reg(inst.args[i], REG_RETVAL + (i - 1));
                    } else {
                        ensure_in_reg(inst.args[i], SCRATCH_REGS[0]);
                        emit("STO (R" + std::to_string(REG_SP) + "), R" +
                                 std::to_string(SCRATCH_REGS[0]),
                             "Push stack arg");
                        emit("SUB R" + std::to_string(REG_SP) + ", 4");
                        stack_arg_size += 4;
                    }
                }

                std::string ret_label = new_asm_label();
                emit("LOD R" + std::to_string(REG_RA) + ", " + ret_label, "Set return address");
                emit("JMP " + get_asm_label(inst.args[0]), "Call function");
                emit_label(ret_label);

                if (stack_arg_size > 0) {
                    emit("ADD R" + std::to_string(REG_SP) + ", " + std::to_string(stack_arg_size),
                         "Cleanup stack args");
                }

                if (inst.result && !inst.result->type->is_void()) {
                    assign_to_reg(inst.result.value(), REG_RETVAL);
                }
                break;
            }

            // I/O
            case IROp::INPUT_I32:
            case IROp::INPUT_I8: {
                emit(inst.op == IROp::INPUT_I32 ? "ITI" : "ITC");
                assign_to_reg(inst.result.value(), REG_IO);
                break;
            }

            case IROp::OUTPUT_I32:
            case IROp::OUTPUT_I8:
            case IROp::OUTPUT_STR: {
                ensure_in_reg(inst.args[0], REG_IO);
                if (inst.op == IROp::OUTPUT_I32)
                    emit("OTI");
                else if (inst.op == IROp::OUTPUT_I8)
                    emit("OTC");
                else
                    emit("OTS");
                break;
            }

            default: throw std::runtime_error("Unknown IROp in AsmGenerator");
        }
    }

    // --- emit ---
    void emit(std::string inst, std::string comment = "") {
        os << "    " << inst;
        if (not comment.empty()) {
            for (int i = 0; i < 24 - (int)inst.length(); ++i) os << ' ';
            os << "# " << comment;
        }
        os << std::endl;
    }
    void emit_label(std::string label) {
        os << label << ':' << std::endl;
    }
    std::string new_asm_label() {
        return "LL" + std::to_string(label_counter++);
    }

    // --- core code ---

    void spill_reg(int reg, std::string reason) {
        if (reg_cache_rev.count(reg)) {
            std::string name_to_spill = reg_cache_rev.at(reg);
            if (!temp_home_map.count(name_to_spill)) {
                throw std::runtime_error("Spill failed: No home for " + name_to_spill);
            }
            int home_offset = temp_home_map.at(name_to_spill);
            emit("STO (R" + std::to_string(REG_FP) + format_offset(home_offset) + "), R" +
                     std::to_string(reg),
                 "Spill " + name_to_spill + " (" + reason + ")");
            reg_cache.erase(name_to_spill);
            reg_cache_rev.erase(reg);
        }
    }

    // 获取内存操作的助记符 (LOD/LDC 或 STO/STC)
    std::string get_mem_op_mnemonic(const IROperand &op, bool is_load = true) {
        IRType *type = op.type;
        if (type->is_pointer()) {
            type = type->get_pointee_type();
        }
        // 假设 i8 (char) 使用 LDC/STC
        if (type->is_char()) {
            return is_load ? "LDC" : "STC";
        }
        // 默认 i32/pointer 使用 LOD/STO
        return is_load ? "LOD" : "STO";
    }

    // 确保 IR 值位于一个寄存器中
    void ensure_in_reg(const IROperand &op, int target_reg) {
        auto target_reg_str = std::to_string(target_reg);

        // Case 1: 立即数
        if (op.op_type == IROperandType::IMM) {
            spill_reg(target_reg, "load imm"); // 确保目标寄存器可用
            emit("LOD R" + target_reg_str + ", " + std::to_string(op.imm_value), "Load immediate");
            return;
        }

        const auto &name = op.name;

        // Case 2: 全局/标签
        if (op.op_type == IROperandType::GLOBAL || op.op_type == IROperandType::LABEL) {
            spill_reg(target_reg, "load addr"); // 确保目标寄存器可用
            std::string label_name = name;
            if (op.op_type == IROperandType::GLOBAL) {
                if (!global_label_map.count(name)) {
                    throw std::runtime_error("Global label not found: " + name);
                }
                label_name = global_label_map.at(name);
            }
            emit("LOD R" + target_reg_str + ", " + label_name, "Load global/label addr");
            return;
        }

        if (op.op_type != IROperandType::REG) {
            throw std::runtime_error("Unexpected operand type in ensure_in_reg");
        }

        // Case 3: REG 操作数

        // Case 3a: 值已在目标寄存器中
        if (reg_cache.count(name) && reg_cache.at(name) == target_reg) {
            return; // 万事大吉
        }

        // Case 3b: 值在其他寄存器中
        if (reg_cache.count(name)) {
            int old_reg = reg_cache.at(name);
            spill_reg(target_reg, "move reg"); // 腾出目标寄存器
            emit("LOD R" + target_reg_str + ", R" + std::to_string(old_reg),
                 "Move " + name + " (cached)");
            // 更新缓存
            reg_cache_rev.erase(old_reg);
            reg_cache[name] = target_reg;
            reg_cache_rev[target_reg] = name;
            return;
        }

        // Case 3c: 值在 alloca 中
        if (alloca_map.count(name)) {
            spill_reg(target_reg, "load alloca addr"); // 腾出目标寄存器
            get_var_address(op, target_reg);           // 加载 %0 的地址到 target_reg
            return;
        }

        // Case 3d: 值未缓存 (在主页中)
        spill_reg(target_reg, "load home"); // 腾出目标寄存器
        if (not temp_home_map.contains(name)) {
            throw std::runtime_error("Temp var has no home: " + name);
        }
        int home_offset = temp_home_map.at(name);
        emit("LOD R" + target_reg_str + ", (R" + std::to_string(REG_FP) +
                 format_offset(home_offset) + ")",
             "Reload " + name + " from home");
        // 更新缓存
        reg_cache[name] = target_reg;
        reg_cache_rev[target_reg] = name;
    }

    // 为结果分配一个寄存器，如有必要就溢出
    void assign_to_reg(const IROperand &result_op, int target_reg) {
        if (result_op.op_type != IROperandType::REG) {
            throw std::runtime_error("Result of instruction must be a REG operand");
        }
        auto name = result_op.name;

        // 1. 溢出目标寄存器中已有的任何内容
        spill_reg(target_reg, "assign");

        // 2. 如果此变量已在 *其他* 寄存器中，清除旧的映射
        if (reg_cache.count(name)) {
            int old_reg = reg_cache.at(name);
            if (old_reg != target_reg) {
                reg_cache_rev.erase(old_reg);
            }
        }

        // 3. 建立新映射
        reg_cache[name] = target_reg;
        reg_cache_rev[target_reg] = name;
        // 调用者现在将发出指令，将新值放入 target_reg
    }

    // 溢出所有缓存的寄存器
    void spill_all_live_regs(std::string reason) {
        if (reg_cache.empty()) return;
        emit("# Spilling all regs: " + reason);
        for (auto const &[name, reg] : reg_cache) {
            int home_offset = temp_home_map.at(name);
            emit("STO (R" + std::to_string(REG_FP) + format_offset(home_offset) + "), R" +
                     std::to_string(reg),
                 "Spill " + name);
        }
        reg_cache.clear();
        reg_cache_rev.clear();
    }

    // 获取 IR 变量地址
    void get_var_address(const IROperand &op, int target_reg, int offset = 0) {
        const auto name = op.name;
        // 必须是 alloca 变量
        auto home_offset_it = alloca_map.find(name);
        if (home_offset_it != alloca_map.end()) {
            int final_offset = home_offset_it->second + offset;
            emit("LOD R" + std::to_string(target_reg) + ", R" + std::to_string(REG_FP) +
                     format_offset(final_offset),
                 "Get address of " + name);
            return;
        }
        throw std::runtime_error("not an alloca var: " + name);
    }

    // 生成符号表查找汇编标签
    void gen_symbol() {
        for (const auto &global : this->module.globals) {
            const auto &name = global.name; // e.g., "@g", "@str_0"
            // 检查是否为字符串字面量
            if (global.type->is_pointer() && global.type->get_pointee_type()->is_char() &&
                !global.init_str.empty()) {
                this->global_label_map.insert({ name,
                                                "STR" + name.substr(1) }); // "@str_0" -> "STRstr_0"
            } else {
                // 否则是全局变量
                this->global_label_map.insert({ name, "VAR" + name.substr(1) }); // "@g" -> "VARg"
            }
        }
        // 函数
        for (const auto &func : this->module.functions) {
            this->global_label_map.insert(
                { func.name, "FUNC" + func.name.substr(1) }); // "@main" -> "FUNCmain"
        }
    }

    // 获取操作符的汇编标签
    std::string get_asm_label(const IROperand &op) {
        const auto &name = op.name;
        if (op.op_type == IROperandType::LABEL) {
            return name; // e.g., "L_1"
        }
        if (op.op_type == IROperandType::GLOBAL) {
            if (global_label_map.count(name)) {
                return global_label_map.at(name); // e.g., "FUNCmain", "VARg"
            }
        }
        throw std::runtime_error("Cannot get label for: " + name);
    }

    // 格式化偏移 给出正负
    std::string format_offset(int offset) {
        if (offset > 0) return " + " + std::to_string(offset);
        if (offset < 0) return " - " + std::to_string(-offset);
        return ""; // 偏移量为 0
    }
};
