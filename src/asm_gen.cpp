#include "asm_gen.hpp"
#include "ir.hpp"
#include <cstddef>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

// main func
void AsmGenerator::generate() {
    // 1. 生成符号表
    gen_symbol();

    // 2. 生成代码段
    os << std::endl << "# --- Text Segment ---" << std::endl;
    emit("LOD R" + std::to_string(REG_SP) + ", 65535", "Init Stack Pointer");
    emit("LOD R" + std::to_string(REG_FP) + ", R" + std::to_string(REG_SP), "Init Frame Pointer");
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

// --- emit ---

void AsmGenerator::emit(std::string inst, std::string comment) {
    os << "   " << inst;
    if (not comment.empty()) {
        for (int i = 0; i < 24 - (int)inst.length(); ++i) {
            os << ' ';
        }
        os << "# " << comment;
    }
    os << std::endl;
}

void AsmGenerator::emit_label(std::string label) {
    os << label << ':' << std::endl;
}

std::string AsmGenerator::new_asm_label() {
    // 使用一个唯一的内部前缀，避免和 IR 的 "L<n>" 标签冲突
    return "LL" + std::to_string(label_counter++);
}

std::string AsmGenerator::get_asm_label(const IROperand &op) {
    std::string name = std::get<std::string>(op.value);
    if (name.rfind("%L", 0) == 0) {
        return name.substr(1);
    }
    if (global_label_map.count(name)) {
        return global_label_map.at(name);
    }
    throw std::runtime_error("Cannot get label for: " + name);
}

void AsmGenerator::gen_symbol() {
    for (const auto &global : this->module.global_vars) {
        // global vars 里面有全局变量和字符串
        auto &name = global.name;
        if (name.at(0) == '@') {
            // 全局变量
            this->global_label_map.insert({ name, "VAR" + name.substr(1) });
            continue;
        }
        if (name.at(0) == '\'') {
            // 字符串字面量
            this->global_label_map.insert({ name, "STR" + name.substr(1) });
            continue;
        }
        throw std::runtime_error("Unknown global symbol when gen symbol: " + name);
    }

    // 函数
    for (const auto &func : this->module.functions) {
        auto &name = func.name;
        this->global_label_map.insert({ name, "FUNC" + name.substr(1) });
    }
}

// --- visitor ---

void AsmGenerator::visit_globals() {
    os << std::endl << "# --- Data Segment ---" << std::endl;
    for (const auto &global : this->module.global_vars) {
        auto &name = global.name;
        if (name.at(0) == '@') {
            emit_label(this->global_label_map.at(name));
            emit("DBN 0, 4", "Global var: " + global.name);
            continue;
        }
        if (name.at(0) == '\'') {
            emit_label(this->global_label_map.at(name));
            std::string dbs = "DBS ";
            for (size_t i = 0; i < global.initializer_str.length(); ++i) {
                dbs += std::to_string(static_cast<int>(global.initializer_str.at(i))) + ", ";
            }
            dbs += "0"; // null terminator
            emit(dbs, global.name);
            continue;
        }
        throw std::runtime_error("Unknown global symbol when visit symbol: " + name);
    }
}

void AsmGenerator::ensure_in_reg(const IROperand &op, int target_reg) {
    if (std::holds_alternative<int>(op.value)) {
        // 字面量
        emit("LOD R" + std::to_string(target_reg) + ", " + std::to_string(std::get<int>(op.value)),
             "Load immediate");
        return;
    }
    if (std::holds_alternative<char>(op.value)) {
        emit("LOD R" + std::to_string(target_reg) + ", " +
                 std::to_string(static_cast<int>(std::get<char>(op.value))),
             "Load immediate char");
        return;
    }

    auto name = std::get<std::string>(op.value);
    auto target_reg_str = std::to_string(target_reg);
    // 已经缓存
    if (reg_cache.contains(name)) {
        int reg = reg_cache.at(name);
        if (reg != target_reg) {
            emit("LOD R" + target_reg_str + ", R" + std::to_string(reg),
                 "Move " + name + " (cached)");
        }
        return;
    }
    // 为全局符号
    if (global_label_map.contains(name)) {
        emit("LOD R" + target_reg_str + ", " + global_label_map.at(name), "Load global addr");
        return;
    }

    // 从主页加载
    if (not temp_home_map.contains(name)) {
        throw std::runtime_error("where is the op live? i dont know");
    }
    int home_offset = temp_home_map.at(name);
    emit("LOD R" + target_reg_str + ", (R" + std::to_string(REG_FP) + format_offset(home_offset) +
             ")",
         "Reload " + name + " from home");
}

void AsmGenerator::assign_to_reg(const IROperand &result_op, int target_reg) {
    auto name = std::get<std::string>(result_op.value);
    // 目前变量所在寄存器
    auto name_in_reg_it = reg_cache.find(name);
    // 目标寄存器占用变量
    auto target_reg_occupied_it = reg_cache_rev.find(target_reg);

    if (name_in_reg_it != reg_cache.end() && name_in_reg_it->second == target_reg) {
        return;
    }
    if (target_reg_occupied_it != reg_cache_rev.end()) {
        auto old_name_in_target_reg = target_reg_occupied_it->second;

        int home_offset = temp_home_map.at(old_name_in_target_reg);
        emit("STO (R" + std::to_string(REG_FP) + format_offset(home_offset) + "), R" +
                 std::to_string(target_reg),
             "Spill " + old_name_in_target_reg);

        reg_cache.erase(old_name_in_target_reg);
    }
    if (name_in_reg_it != reg_cache.end()) {
        int old_reg_for_name = name_in_reg_it->second;
        reg_cache_rev.erase(old_reg_for_name);
    }

    reg_cache.insert_or_assign(name, target_reg);
    reg_cache_rev.insert_or_assign(target_reg, name);
}

void AsmGenerator::spill_all_live_regs(std::string reason) {
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

void AsmGenerator::get_var_address(const IROperand &op, int target_reg, int offset) {
    const auto name = std::get<std::string>(op.value);
    auto home_offset = temp_home_map.find(name);
    if (home_offset != temp_home_map.end()) {
        // 注意：这里的 + 语义不同，它是计算地址的一部分
        // 但我们假设 get_var_address 仅用于 alloca，并且 offset 总是 0
        if (offset != 0) {
            throw std::runtime_error("get_var_address with non-zero offset not fully implemented "
                                     "for pretty formatting");
        }
        emit("LOD R" + std::to_string(target_reg) + ", R" + std::to_string(REG_FP) +
                 format_offset(home_offset->second),
             "get address");
        return;
    }
    throw std::runtime_error("not an alloca var: " + name);
}

void AsmGenerator::visit_function(const IRFunction &func) {
    const auto func_name = func.name;
    os << std::endl << "# --- Function: " << func_name << " ---" << std::endl;
    // 函数信息
    emit_label(global_label_map.at(func_name));

    // 栈布局
    this->alloca_map.clear();
    this->temp_home_map.clear();
    this->reg_cache.clear();
    this->reg_cache_rev.clear();

    auto local_stack_size = 0;
    // FP + 4 (RA)
    // FP + 8 (Old FP)
    auto param_stack_offset = 12;

    // 映射参数主页
    for (size_t i = 0; i < func.params.size(); ++i) {
        const auto &param_name = std::get<std::string>(func.params.at(i).value);
        if (i < MAX_REGS_FOR_PARAMS) {
            local_stack_size += 4;
            this->temp_home_map.insert({ param_name, -local_stack_size });
        } else {
            this->temp_home_map.insert({ param_name, param_stack_offset });
            param_stack_offset += 4;
        }
    }

    // 映射alloca和临时变量的主页
    for (const auto &block : func.blocks) {
        for (const auto &inst : block.instructions) {
            if (inst.op == IROp::ALLOCA) {
                auto name = std::get<std::string>(inst.result.value);
                local_stack_size += 4;
                this->alloca_map.insert({ name, -local_stack_size });
                continue;
            }
            if (inst.result.type != IRType::VOID) {
                auto name = std::get<std::string>(inst.result.value);
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

    // ? TODO 优化
    for (size_t i = 0; i < func.params.size() && i < MAX_REGS_FOR_PARAMS; ++i) {
        const auto &param_name = std::get<std::string>(func.params[i].value);
        int param_reg = REG_RETVAL + i;
        int offset = temp_home_map.at(param_name);
        emit("STO (R" + std::to_string(REG_FP) + format_offset(offset) + "), R" +
                 std::to_string(param_reg),
             "Store param " + param_name + " to home");
    }

    // 访问指令
    for (const auto &block : func.blocks) {
        for (const auto &inst : block.instructions) {
            visit_instruction(inst);
        }
    }
}

void AsmGenerator::visit_instruction(const IRInstruction &inst) {
    switch (inst.op) {
        case IROp::LABEL:
            spill_all_live_regs("Label");
            emit_label(get_asm_label(inst.operands[0]));
            break;

        case IROp::RET: {
            if (!inst.operands.empty()) {
                ensure_in_reg(inst.operands[0], REG_RETVAL);
            }
            emit("LOD R" + std::to_string(REG_SP) + ", R" + std::to_string(REG_FP), "Restore SP");
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
            emit("JMP " + get_asm_label(inst.operands[0]));
            break;

        case IROp::TEST:
            ensure_in_reg(inst.operands[0], SCRATCH_REGS[0]);
            ensure_in_reg(inst.operands[1], SCRATCH_REGS[1]);
            emit("SUB R" + std::to_string(SCRATCH_REGS[0]) + ", R" +
                     std::to_string(SCRATCH_REGS[1]),
                 "L - R");
            emit("TST R" + std::to_string(SCRATCH_REGS[0]));
            break;

        case IROp::BRZ:
            spill_all_live_regs("BRZ");
            emit("JEZ " + get_asm_label(inst.operands[0]));
            break;
        case IROp::BRLT:
            spill_all_live_regs("BRLT");
            emit("JLZ " + get_asm_label(inst.operands[0]));
            break;
        case IROp::BRGT:
            spill_all_live_regs("BRGT");
            emit("JGZ " + get_asm_label(inst.operands[0]));
            break;

        case IROp::ALLOCA:
            // 已处理
            break;

        case IROp::LOAD: { // dest(%n) = LOAD src_ptr(%p)
            std::string src_name = std::get<std::string>(inst.operands[0].value);
            std::string op_mnemonic = get_mem_op_mnemonic(inst.operands[0], true);

            // 为 dest 分配寄存器 结果将在 R8
            assign_to_reg(inst.result, SCRATCH_REGS[0]);

            if (alloca_map.count(src_name)) {
                // 从 stack加载
                int src_offset = alloca_map.at(src_name);
                emit(op_mnemonic + " R" + std::to_string(SCRATCH_REGS[0]) + ", (R" +
                         std::to_string(REG_FP) + format_offset(src_offset) + ")",
                     "Load from alloca");
            } else if (global_label_map.count(src_name)) {
                // 从 global加载
                // 将全局变量的地址加载到 R9
                ensure_in_reg(inst.operands[0], SCRATCH_REGS[1]);
                // 从 (R9) 中的地址加载值到 R8
                emit(op_mnemonic + " R" + std::to_string(SCRATCH_REGS[0]) + ", (R" +
                         std::to_string(SCRATCH_REGS[1]) + ")",
                     "Load from global var");
            } else {
                throw std::runtime_error("LOAD source must be an 'alloca' or 'global' variable: " +
                                         src_name);
            }
            break;
        }

        case IROp::STORE: { // STORE src_val(%n), dest_ptr(%p)
            // 确保 src_val 在寄存器中 (R8)
            ensure_in_reg(inst.operands[0], SCRATCH_REGS[0]);

            std::string dest_name = std::get<std::string>(inst.operands[1].value);
            std::string op_mnemonic = get_mem_op_mnemonic(inst.operands[1], false);

            if (alloca_map.count(dest_name)) {
                // 存储到栈
                int dest_offset = alloca_map.at(dest_name);
                emit(op_mnemonic + " (R" + std::to_string(REG_FP) + format_offset(dest_offset) +
                         "), R" + std::to_string(SCRATCH_REGS[0]),
                     "Store to alloca");
            } else if (global_label_map.count(dest_name)) {
                // 存储到 global
                // 将全局变量的地址加载到 R9
                ensure_in_reg(inst.operands[1], SCRATCH_REGS[1]);
                // 将 R8 中的值存储到 (R9) 中的地址
                emit(op_mnemonic + " (R" + std::to_string(SCRATCH_REGS[1]) + "), R" +
                         std::to_string(SCRATCH_REGS[0]),
                     "Store to global var");
            } else {
                throw std::runtime_error(
                    "STORE destination must be an 'alloca' or 'global' variable: " + dest_name);
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
            else
                op_str = "DIV";

            ensure_in_reg(inst.operands[0], SCRATCH_REGS[0]);
            ensure_in_reg(inst.operands[1], SCRATCH_REGS[1]);

            assign_to_reg(inst.result, SCRATCH_REGS[2]);

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

            for (size_t i = 1; i < inst.operands.size(); ++i) {
                if (i - 1 < MAX_REGS_FOR_PARAMS) {
                    ensure_in_reg(inst.operands[i], REG_RETVAL + (i - 1));
                } else {
                    ensure_in_reg(inst.operands[i], SCRATCH_REGS[0]);
                    emit("STO (R" + std::to_string(REG_SP) + "), R" +
                             std::to_string(SCRATCH_REGS[0]),
                         "Push stack arg");
                    emit("SUB R" + std::to_string(REG_SP) + ", 4");
                    stack_arg_size += 4;
                }
            }

            std::string ret_label = new_asm_label();
            emit("LOD R" + std::to_string(REG_RA) + ", " + ret_label, "Set return address");
            emit("JMP " + get_asm_label(inst.operands[0]), "Call function");
            emit_label(ret_label);

            if (stack_arg_size > 0) {
                emit("ADD R" + std::to_string(REG_SP) + ", " + std::to_string(stack_arg_size),
                     "Cleanup stack args");
            }

            if (inst.result.type != IRType::VOID) {
                assign_to_reg(inst.result, REG_RETVAL);
            }
            break;
        }

        // I/O
        case IROp::INPUT_INT:
        case IROp::INPUT_CHAR: {
            emit(inst.op == IROp::INPUT_INT ? "ITI" : "ITC");
            assign_to_reg(inst.result, REG_IO);
            break;
        }

        case IROp::OUTPUT_INT:
        case IROp::OUTPUT_CHAR:
        case IROp::OUTPUT_STRING: {
            ensure_in_reg(inst.operands[0], REG_IO);
            if (inst.op == IROp::OUTPUT_INT)
                emit("OTI");
            else if (inst.op == IROp::OUTPUT_CHAR)
                emit("OTC");
            else
                emit("OTS");
            break;
        }

        default: throw std::runtime_error("Unknown IROp in AsmGenerator");
    }
}

std::string AsmGenerator::get_mem_op_mnemonic(const IROperand &op, bool is_load) {
    IRType type = op.type;
    if (type == IRType::PTR) {
        type = op.pointee_type;
    }
    if (type == IRType::I8) {
        return is_load ? "LDC" : "STC";
    }
    return is_load ? "LOD" : "STO";
}

std::string AsmGenerator::format_offset(int offset) {
    if (offset > 0) {
        return " + " + std::to_string(offset);
    }
    if (offset < 0) {
        return " - " + std::to_string(-offset);
    }
    return ""; // 偏移量为 0
}
