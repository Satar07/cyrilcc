#pragma once

#include "ir.hpp"
#include <array>
#include <ostream>
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

    void generate();

  private:
    IRModule &module;
    std::ostream &os;

    // --- 状态量 ---

    // 符号名 -> 汇编标签
    std::unordered_map<std::string, std::string> global_label_map;
    // alloca变量 -> 栈偏移量
    std::unordered_map<std::string, int> alloca_map;
    // 临时变量 -> 主页栈偏移量
    std::unordered_map<std::string, int> temp_home_map;

    // 临时变量 -> 物理寄存器
    std::unordered_map<std::string, int> reg_cache;
    // 物理寄存器 -> 临时变量
    std::unordered_map<int, std::string> reg_cache_rev;

    // 当前栈帧大小
    int current_frame_size = 0;

    // 标签计数器
    int label_counter = 0;

    // --- visitor ---
    void visit_globals();
    void visit_function(const IRFunction &func);
    void visit_instruction(const IRInstruction &inst);

    // --- emit ---
    void emit(std::string inst, std::string comment = "");
    void emit_label(std::string label);
    std::string new_asm_label();

    // --- core code ---
    // 贪心策略

    // 将操作数加载到目标寄存器
    void load_operand(const IROperand &op, int target_reg);

    // 获取内存操作的助记符 (LOD/LDC 或 STO/STC)
    std::string get_mem_op_mnemonic(const IROperand &op, bool is_load = true);

    // 确保 IR 值位于一个寄存器中
    void ensure_in_reg(const IROperand &op, int target_reg);

    // 为结果分配一个寄存器，如果有必要就溢出
    void assign_to_reg(const IROperand &result_op, int target_reg);

    // 溢出所有缓存的寄存器
    void spill_all_live_regs(std::string reason);

    // 获取 IR 变量地址
    void get_var_address(const IROperand &op, int target_reg, int offset = 0);

    // 生成符号表查找汇编标签
    void gen_symbol();

    // 获取操作符的汇编标签
    std::string get_asm_label(const IROperand &op);

    // 格式化偏移 给出正负
    std::string format_offset(int offset);
};
