#include "ir.hpp"
#include <cstddef>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

// --- IR Dumper ---

// 辅助函数：打印类型
static std::string type_to_string(IRType type) {
    switch (type) {
        case IRType::VOID: return "void";
        case IRType::I32: return "i32";
        case IRType::I8: return "i8";
        case IRType::PTR: return "ptr";
        case IRType::LABEL: return "label";
        default: throw std::runtime_error("Unknown IRType in dumper");
    }
}

// 辅助函数：打印操作数
static std::string operand_to_string(const IROperand &op) {
    std::stringstream ss;
    if (op.type == IRType::PTR) {
        ss << "ptr ";
    }

    if (op.type != IRType::VOID and op.type != IRType::LABEL) {
        if (op.type != IRType::PTR) {
            ss << type_to_string(op.type) << " ";
        } else {
            ss << type_to_string(op.pointee_type) << " ";
        }
    }

    if (std::holds_alternative<int>(op.value)) {
        ss << std::get<int>(op.value);
    } else if (std::holds_alternative<char>(op.value)) {
        ss << "'" << std::get<char>(op.value) << "'";
    } else if (std::holds_alternative<std::string>(op.value)) {
        ss << std::get<std::string>(op.value);
    } else {
        ss << "[INVALID_OPERAND]";
    }

    return ss.str();
}

// 指令到 OpCode 字符串的映射
static const std::map<IROp, std::string> op_to_string_map = {
    { IROp::RET, "ret" },
    { IROp::BR, "br" },
    { IROp::TEST, "test" },
    { IROp::BRZ, "brz" },
    { IROp::BRLT, "brlt" },
    { IROp::BRGT, "brgt" },
    { IROp::ALLOCA, "alloca" },
    { IROp::LOAD, "load" },
    { IROp::STORE, "store" },
    { IROp::MOV, "mov" },
    { IROp::ADD, "add" },
    { IROp::SUB, "sub" },
    { IROp::MUL, "mul" },
    { IROp::DIV, "div" },
    { IROp::CALL, "call" },
    { IROp::INPUT_INT, "input_int" },
    { IROp::INPUT_CHAR, "input_char" },
    { IROp::OUTPUT_INT, "output_int" },
    { IROp::OUTPUT_CHAR, "output_char" },
    { IROp::OUTPUT_STRING, "output_string" },
    { IROp::LABEL, "label" } // 伪指令
};

// 打印单个指令
static void dump_instruction(std::ostream &os, const IRInstruction &inst) {

    // 伪指令 LABEL
    if (inst.op == IROp::LABEL) {
        os << operand_to_string(inst.operands[0]) << ":" << std::endl;
        return;
    }
    os << "  "; // 缩进
    // 有结果就放在等号左边
    if (inst.result.type != IRType::VOID) {
        os << operand_to_string(inst.result) << " = ";
    }

    // OpCode
    os << op_to_string_map.at(inst.op);

    // 操作数
    for (size_t i = 0; i < inst.operands.size(); ++i) {
        os << (i == 0 ? " " : ", ");
        os << operand_to_string(inst.operands[i]);
    }

    os << std::endl;
}

// 打印基本块
static void dump_basic_block(std::ostream &os, const IRBasicBlock &block) {
    for (const auto &inst : block.instructions) {
        dump_instruction(os, inst);
    }
}

// 打印函数
static void dump_function(std::ostream &os, const IRFunction &func) {
    os << "define " << type_to_string(func.return_type) << " " << func.name << "(";
    for (size_t i = 0; i < func.params.size(); ++i) {
        os << (i == 0 ? "" : ", ");
        os << operand_to_string(func.params[i]);
    }
    os << ") {" << std::endl;

    for (const auto &block : func.blocks) {
        dump_basic_block(os, block);
    }

    os << "}" << std::endl << std::endl;
}

// 打印全局变量
static void dump_global_var(std::ostream &os, const IRGlobalVar &global) {
    os << global.name << " = global " << type_to_string(global.type);
    if (!global.initializer_str.empty()) {
        // 简单地转义字符串
        std::string escaped_str;
        for (char c : global.initializer_str) {
            if (c == '\n')
                escaped_str += "\\n";
            else if (c == '\t')
                escaped_str += "\\t";
            else if (c == '\"')
                escaped_str += "\\\"";
            else if (c == '\\')
                escaped_str += "\\\\";
            else
                escaped_str += c;
        }
        os << " c\"" << escaped_str << "\\00\"";
    }
    os << std::endl;
}

// IRModule::dump 的实现
void IRModule::dump(std::ostream &os) {
    os << "--- IR Module ---" << std::endl;

    // 打印全局变量
    if (!global_vars.empty()) {
        os << std::endl << "--- Globals ---" << std::endl;
        for (const auto &global : global_vars) {
            dump_global_var(os, global);
        }
    }

    // 打印函数
    if (!functions.empty()) {
        os << std::endl << "--- Functions ---" << std::endl;
        for (const auto &func : functions) {
            dump_function(os, func);
        }
    }

    os << "--- End Module ---" << std::endl;
}
