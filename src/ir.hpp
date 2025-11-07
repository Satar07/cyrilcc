#pragma once

#include "ast.hpp"  // 包含 ast.hpp
#include "type.hpp" // 包含 type.hpp
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ========================================================
// --- IR 结构定义 ---
// ========================================================

// --- 操作数类型 ---
enum class IROperandType { IMM, REG, LABEL, GLOBAL };

struct IROperand {
    IROperandType op_type;
    IRType *type = nullptr; // 使用指针指向唯一的类型实例
    int imm_value = 0;
    std::string name; // 用于 REG (%0), LABEL (L1), GLOBAL (@g)

    IROperand() = default;
    IROperand(IROperandType ot, IRType *t) : op_type(ot), type(t) {}

    static IROperand create_imm(int val, IRType *type) {
        IROperand op(IROperandType::IMM, type);
        op.imm_value = val;
        return op;
    }
    static IROperand create_reg(std::string name, IRType *type) {
        IROperand op(IROperandType::REG, type);
        op.name = std::move(name);
        return op;
    }
    static IROperand create_label(std::string name) {
        IROperand op(IROperandType::LABEL, IRType::get_void());
        op.name = std::move(name);
        return op;
    }
    static IROperand create_global(std::string name, IRType *type) {
        IROperand op(IROperandType::GLOBAL, type);
        op.name = std::move(name);
        return op;
    }
    bool is_valid() const {
        return type != nullptr;
    }

    std::string to_string() const {
        switch (op_type) {
            case IROperandType::IMM: return std::to_string(imm_value);
            case IROperandType::REG: return name;
            case IROperandType::LABEL: return name;
            case IROperandType::GLOBAL: return name;
        }
        return "<?>";
    }
};

// --- IR 指令集 ---
enum class IROp {
    // 终结
    RET,
    // 无条件跳转
    BR,
    // 条件跳转 (TEST/CMP + Branch)
    BRZ,
    BRLT,
    BRGT,
    // 比较 (设置标志位)
    TEST, // (在我们的实现中，TEST 被内置到条件跳转中)
    // 内存
    ALLOCA, // 分配栈空间
    LOAD,   // 从内存加载
    STORE,  // 存储到内存
    GEP,    // 获取元素指针
    // 算术
    ADD,
    SUB,
    MUL,
    DIV,
    // 函数
    CALL,
    // I/O 扩展指令
    INPUT_I32,
    INPUT_I8,
    OUTPUT_I32,
    OUTPUT_I8,
    OUTPUT_STR,
    // 伪指令
    LABEL,
    PHI, // φ节点
};

// 辅助函数：将 IROp 转换为字符串
inline std::string op_to_string(IROp op) {
    switch (op) {
        case IROp::RET: return "ret";
        case IROp::BR: return "br";
        case IROp::BRZ: return "brz";
        case IROp::BRLT: return "brlt";
        case IROp::BRGT: return "brgt";
        case IROp::TEST: return "test";
        case IROp::ALLOCA: return "alloca";
        case IROp::LOAD: return "load";
        case IROp::STORE: return "store";
        case IROp::GEP: return "getelementptr";
        case IROp::ADD: return "add";
        case IROp::SUB: return "sub";
        case IROp::MUL: return "mul";
        case IROp::DIV: return "div";
        case IROp::CALL: return "call";
        case IROp::INPUT_I32: return "input_i32";
        case IROp::INPUT_I8: return "input_i8";
        case IROp::OUTPUT_I32: return "output_i32";
        case IROp::OUTPUT_I8: return "output_i8";
        case IROp::OUTPUT_STR: return "output_str";
        case IROp::LABEL: return "label";
        case IROp::PHI: return "phi";
    }
    return "unknown_op";
}

struct IRInstruction {
    IROp op;
    std::vector<IROperand> args;
    std::optional<IROperand> result;
    IRInstruction(IROp o, std::vector<IROperand> a = {}, std::optional<IROperand> r = std::nullopt)
        : op(o), args(std::move(a)), result(std::move(r)) {}

    void dump(std::ostream &os) const {
        // 1. 打印缩进
        os << "  ";

        // 2. 打印结果 (e.g., "%1 = ")
        if (result) {
            os << result->to_string() << " " << result->type->to_string() << " = ";
        }

        // 3. 打印操作码 (e.g., "add")
        os << op_to_string(op);

        // 4. 打印操作数 (e.g., " i32 %a, i32 5")
        for (const auto &arg : args) {
            os << " ";

            // 特殊处理：跳转目标 (label) 不显示类型
            if (arg.op_type == IROperandType::LABEL && op != IROp::LABEL) {
                os << "label " << arg.to_string();
                continue;
            }
            // phi
            if (op == IROp::PHI) {
                for (size_t i = 0; i < args.size(); i += 2) {
                    os << " [ " << args[i].to_string() << ", " << args[i + 1].to_string() << " ]";
                    if (i + 2 < args.size()) os << ",";
                }
                continue;
            }
            os << arg.to_string() << " " << arg.type->to_string();
        }
    }
};

// --- 基本块 ---
struct IRBasicBlock {
    std::string label;
    std::vector<IRInstruction> insts;

    std::vector<IRBasicBlock *> successors;
    std::vector<IRBasicBlock *> predecessors;

    IRBasicBlock *idom = nullptr;                     // 本块的支配节点
    std::vector<IRBasicBlock *> dom_child;            // 本块在支配树中的孩子节点
    std::unordered_set<IRBasicBlock *> dom_frontiers; // 本块的支配边界

    IRBasicBlock(std::string l) : label(std::move(l)) {}
};

// --- 函数定义 ---
struct IRFunction {
    std::string name;
    IRType *ret_type;
    std::vector<IROperand> params;                           // 参数列表 (虚拟寄存器)
    std::vector<IRBasicBlock> blocks;                        // 基本块列表
    std::unordered_map<std::string, IROperand> symbol_table; // 局部变量表 (映射到栈指针)
    int vreg_cnt = 0;
    IRFunction(std::string n, IRType *rt) : name(std::move(n)), ret_type(rt) {}

    IROperand new_reg(IRType *type) {
        return IROperand::create_reg("%" + std::to_string(vreg_cnt++), type);
    }
};

// --- 全局变量 ---
struct IRGlobalVar {
    std::string name;
    IRType *type;
    std::string init_str; // 仅用于字符串字面量
    IRGlobalVar(std::string n, IRType *t) : name(std::move(n)), type(t) {}
    std::string escaped_init_str() const {
        std::string s;
        for (char c : init_str) {
            if (c == '\n')
                s += "\\n";
            else if (c == '\t')
                s += "\\t";
            else
                s += c;
        }
        return s;
    }
};

// --- 模块 (Top Level) ---
struct IRModule {
    std::vector<IRGlobalVar> globals;
    std::vector<IRFunction> functions;
    std::unordered_map<std::string, IROperand> global_symbols;

    // 完整的 Dump 实现
    void dump(std::ostream &os) const {
        os << "; --- Global Variables ---\n";
        for (const auto &g : globals) {
            os << g.name << " = global " << g.type->to_string();
            if (!g.init_str.empty()) {
                os << " \"" << g.escaped_init_str() << "\"";
            }
            os << "\n";
        }
        os << "\n";

        for (const auto &f : functions) {
            os << "define " << f.ret_type->to_string() << " " << f.name << "(";
            for (size_t i = 0; i < f.params.size(); ++i) {
                os << f.params[i].type->to_string() << " " << f.params[i].to_string();
                if (i < f.params.size() - 1) os << ", ";
            }
            os << ") {\n";

            for (const auto &b : f.blocks) {
                // 第一个指令 (LABEL) 比较特殊
                if (!b.insts.empty() && b.insts[0].op == IROp::LABEL) {
                    os << b.label << ":\n";
                } else {
                    os << ";" << b.label << " (no label):\n";
                }

                for (const auto &i : b.insts) {
                    if (i.op == IROp::LABEL) continue; // 已经打印过
                    i.dump(os);
                    os << "\n";
                }

                os << " ; Predecessors: ";
                if (b.predecessors.empty()) {
                    os << "<none>";
                } else {
                    for (size_t p = 0; p < b.predecessors.size(); ++p) {
                        os << b.predecessors[p]->label
                           << (p < b.predecessors.size() - 1 ? ", " : "");
                    }
                }
                os << "\n";

                os << " ; Successors: ";
                if (b.successors.empty()) {
                    os << "<none>";
                } else {
                    for (size_t s = 0; s < b.successors.size(); ++s) {
                        os << b.successors[s]->label << (s < b.successors.size() - 1 ? ", " : "");
                    }
                }
                os << "\n";
                os << " ; Immediate Dominator: ";
                if (b.idom) {
                    os << b.idom->label;
                } else {
                    os << "<none>";
                }
                os << "\n";

                os << " ; Dominator Children: ";
                if (b.dom_child.empty()) {
                    os << "<none>";
                } else {
                    for (size_t c = 0; c < b.dom_child.size(); ++c) {
                        os << b.dom_child[c]->label << (c < b.dom_child.size() - 1 ? ", " : "");
                    }
                }
                os << "\n";
                os << " ; Dominance Frontiers: ";
                if (b.dom_frontiers.empty()) {
                    os << "<none>";
                } else {
                    size_t count = 0;
                    for (const auto &df : b.dom_frontiers) {
                        os << df->label << (count < b.dom_frontiers.size() - 1 ? ", " : "");
                        count++;
                    }
                }
                os << "\n";
            }
            os << "}\n\n";
        }
    }
};

// ========================================================
// --- IR 生成器 ---
// ========================================================
class IRGenerator {
  public:
    IRModule module;

    IRGenerator(std::unique_ptr<ASTNode> &root) {
        if (root) dispatch(root.get());
    }

  private:
    // --- 状态 ---
    IRFunction *cur_func = nullptr;
    IRBasicBlock *cur_block = nullptr;
    int label_cnt = 0;
    int str_cnt = 0;
    std::vector<std::pair<std::string, std::string>> loop_stack; // <continue_lbl, break_lbl>

    // --- 辅助工具 ---
    std::string new_label(const std::string &prefix = "L") {
        return prefix + std::to_string(label_cnt++);
    }
    IROperand new_reg(IRType *type) {
        if (!cur_func) throw std::runtime_error("Cannot create register outside a function");
        return cur_func->new_reg(type);
    }

    void create_block(std::string label) {
        if (!cur_func) throw std::runtime_error("Cannot create block outside a function");
        cur_func->blocks.emplace_back(label);
        cur_block = &cur_func->blocks.back();
        emit(IROp::LABEL, { IROperand::create_label(label) });
    }

    void
    emit(IROp op, std::vector<IROperand> args = {}, std::optional<IROperand> res = std::nullopt) {
        if (!cur_block) throw std::runtime_error("Cannot emit outside a basic block");
        cur_block->insts.emplace_back(op, std::move(args), std::move(res));
    }

    // --- 分发 ---
    void dispatch(ASTNode *node) {
        if (!node) return;
        // 顶层
        if (auto n = dynamic_cast<ProgramNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<FunctionNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<VariableDeclarationListNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<StructDefinitionNode *>(node)) {
            visit(n);
            return;
        }
        // 语句
        if (auto n = dynamic_cast<IfStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<WhileStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<ForStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<SwitchStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<CaseStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<DefaultStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<CaseBlockStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<ReturnStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<BreakStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<ContinueStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<InputStatementNode *>(node)) {
            visit(n);
            return;
        }
        if (auto n = dynamic_cast<OutputStatementNode *>(node)) {
            visit(n);
            return;
        }
        // 表达式语句
        if (auto n = dynamic_cast<ExpressionNode *>(node)) {
            dispatch_expr(n);
            return;
        }
        // 忽略其他 (如 ParameterDeclarationNode，由 FunctionNode 处理)
    }

    IROperand dispatch_expr(ASTNode *node) {
        if (auto n = dynamic_cast<AssignmentNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<BinaryOpNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<UnaryOpNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<ArrayIndexNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<MemberAccessNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<FunctionCallNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<VariableReferenceNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<IntegerLiteralNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<CharacterLiteralNode *>(node)) return visit(n);
        if (auto n = dynamic_cast<StringLiteralNode *>(node)) return visit(n);
        throw std::runtime_error("Unknown expression node");
    }

    // --- 符号/LValue ---
    IROperand get_symbol_ptr(const std::string &name) {
        if (cur_func) {
            auto it = cur_func->symbol_table.find(name);
            if (it != cur_func->symbol_table.end()) return it->second;
        }
        auto it = module.global_symbols.find(name);
        if (it != module.global_symbols.end()) return it->second;
        throw std::runtime_error("Symbol not found: " + name);
    }

    IROperand get_lvalue_addr(ASTNode *node) {
        if (auto var = dynamic_cast<VariableReferenceNode *>(node)) {
            return get_symbol_ptr(var->name);
        }
        if (auto deref = dynamic_cast<UnaryOpNode *>(node)) {
            if (deref->op == UnaryOpKind::DEREF) {
                return dispatch_expr(deref->operand.get()); // *p 的地址就是 p 的值
            }
        }
        if (auto member = dynamic_cast<MemberAccessNode *>(node)) {
            IROperand base_ptr = get_lvalue_addr(member->object.get());
            IRType *base_type = base_ptr.type->get_pointee_type();
            if (!base_type->is_struct()) throw std::runtime_error("Member access on non-struct");

            int field_index = base_type->get_field_index(member->member_name);

            IRType *field_type = base_type->get_field(member->member_name)->type;
            IROperand res_ptr = new_reg(IRType::get_pointer(field_type));

            // GEP: %res_ptr = gep %base_ptr, i32 0, i32 <field_index>
            emit(IROp::GEP,
                 { base_ptr, IROperand::create_imm(0, IRType::get_i32()),
                   IROperand::create_imm(field_index, IRType::get_i32()) },
                 res_ptr);
            return res_ptr;
        }
        if (auto idx = dynamic_cast<ArrayIndexNode *>(node)) {
            IROperand base_ptr = get_lvalue_addr(idx->array.get());
            IROperand index_val = dispatch_expr(idx->index.get());

            IRType *base_type = base_ptr.type->get_pointee_type();
            if (!base_type->is_array()) throw std::runtime_error("Array index on non-array");

            IRType *elem_type = base_type->get_array_element_type();
            IROperand res_ptr = new_reg(IRType::get_pointer(elem_type));

            // GEP: %res_ptr = gep %base_ptr, i32 0, i32 %index_val
            emit(IROp::GEP, { base_ptr, IROperand::create_imm(0, IRType::get_i32()), index_val },
                 res_ptr);
            return res_ptr;
        }
        throw std::runtime_error("Expression is not an lvalue");
    }

    // --- 条件跳转 ---
    void
    visit_condition(ASTNode *cond, const std::string &true_label, const std::string &false_label) {
        if (auto bin_op = dynamic_cast<BinaryOpNode *>(cond)) {
            IROperand lhs = dispatch_expr(bin_op->left.get());
            IROperand rhs = dispatch_expr(bin_op->right.get());
            emit(IROp::TEST, { lhs, rhs });

            switch (bin_op->op) {
                case BinaryOpKind::LT:
                    emit(IROp::BRLT, { IROperand::create_label(true_label) });
                    emit(IROp::BR,
                         { IROperand::create_label(false_label) }); // else goto false_label;
                    break;
                case BinaryOpKind::GT:
                    emit(IROp::BRGT, { IROperand::create_label(true_label) });
                    emit(IROp::BR,
                         { IROperand::create_label(false_label) }); // else goto false_label;
                    break;
                case BinaryOpKind::EQ: // if (lhs == rhs) goto true_label;
                    emit(IROp::BRZ, { IROperand::create_label(true_label) });
                    emit(IROp::BR,
                         { IROperand::create_label(false_label) }); // else goto false_label;
                    break;
                case BinaryOpKind::NE: // if (lhs != rhs) ...
                    emit(IROp::BRZ, { IROperand::create_label(false_label) });
                    emit(IROp::BR, { IROperand::create_label(true_label) });
                    break;
                case BinaryOpKind::LE: // if (lhs <= rhs) ...
                    emit(IROp::BRGT, { IROperand::create_label(false_label) });
                    emit(IROp::BR, { IROperand::create_label(true_label) });
                    break;
                case BinaryOpKind::GE: // if (lhs >= rhs) ...
                    emit(IROp::BRLT, { IROperand::create_label(false_label) });
                    emit(IROp::BR, { IROperand::create_label(true_label) });
                    break;

                default: goto generic_cond; // 非比较运算
            }
            return; // 完成二元操作处理
        }
    generic_cond:
        IROperand val = dispatch_expr(cond);
        emit(IROp::TEST, { val, IROperand::create_imm(0, IRType::get_i32()) });
        emit(IROp::BRZ, { IROperand::create_label(false_label) });
        emit(IROp::BR, { IROperand::create_label(true_label) });
    }

    // --- 节点 Visit 方法 ---
    void visit(ProgramNode *node) {
        // Pass 1: 注册全局符号
        for (auto &def : node->definitions->nodes) {
            if (auto fn = dynamic_cast<FunctionNode *>(def.get())) {
                module.global_symbols[fn->name] = IROperand::create_global("@" + fn->name,
                                                                           fn->return_type);
            } else if (auto vlist = dynamic_cast<VariableDeclarationListNode *>(def.get())) {
                for (auto &vdef_node : vlist->declarations->nodes) {
                    auto vdef = static_cast<VariableDefinitionNode *>(vdef_node.get());
                    std::string gname = "@" + vdef->name;
                    module.globals.emplace_back(gname, vdef->type);
                    module.global_symbols[vdef->name] =
                        IROperand::create_global(gname, IRType::get_pointer(vdef->type));
                }
            }
        }
        // Pass 2: 生成函数体
        for (auto &def : node->definitions->nodes) {
            if (dynamic_cast<FunctionNode *>(def.get())) {
                dispatch(def.get());
            }
        }
    }

    void visit(FunctionNode *node) {
        module.functions.emplace_back("@" + node->name, node->return_type);
        cur_func = &module.functions.back();

        create_block(new_label("entry"));

        for (auto &param_node : node->params->nodes) {
            auto param = static_cast<ParameterDeclarationNode *>(param_node.get());
            IROperand arg_val = new_reg(param->type);
            cur_func->params.push_back(arg_val); // 记录参数值

            IROperand ptr = new_reg(IRType::get_pointer(param->type));
            emit(IROp::ALLOCA, {}, ptr);
            emit(IROp::STORE, { arg_val, ptr });
            cur_func->symbol_table[param->name] = ptr; // 符号表存指针
        }

        for (auto &stmt : node->body->nodes) dispatch(stmt.get());

        // 确保有返回
        bool last_block_terminated = false;
        if (cur_block && !cur_block->insts.empty()) {
            // 检查最后一条指令
            IROp last_op = cur_block->insts.back().op;
            if (last_op == IROp::RET || last_op == IROp::BR) {
                last_block_terminated = true;
            }
        }

        // 如果最后一个块是 "unreachable" (即只有 label)
        // 并且它不是 entry 块，我们认为它已经被前一个块终止了
        if (cur_block && cur_block->insts.size() == 1 && cur_block->insts[0].op == IROp::LABEL &&
            cur_func->blocks.size() > 1) {
            last_block_terminated = true;
        }

        if (!last_block_terminated) {
            emit(IROp::RET,
                 node->return_type->is_void()
                     ? std::vector<IROperand>{}
                     : std::vector<IROperand>{ IROperand::create_imm(0, IRType::get_i32()) });
        }
        // cur_func->build_cfg();
        // cur_func->compute_dominators();
        // cur_func->compute_dominance_frontiers();
        cur_func = nullptr;
    }

    void visit(VariableDeclarationListNode *node) {
        if (!cur_func) return; // 全局变量已在 ProgramNode 处理
        for (auto &vdef_node : node->declarations->nodes) {
            auto vdef = static_cast<VariableDefinitionNode *>(vdef_node.get());
            IROperand ptr = new_reg(IRType::get_pointer(vdef->type));
            emit(IROp::ALLOCA, {}, ptr);
            cur_func->symbol_table[vdef->name] = ptr;
        }
    }

    void visit(StructDefinitionNode *_) {}

    void visit(IfStatementNode *node) {
        std::string true_l = new_label("iftrue");
        std::string false_l = node->else_branch ? new_label("ifelse") : new_label("ifend");
        std::string end_l = node->else_branch ? new_label("ifend") : false_l;

        visit_condition(node->condition.get(), true_l, false_l);

        create_block(true_l);
        for (auto &s : node->then_branch->nodes) dispatch(s.get());
        emit(IROp::BR, { IROperand::create_label(end_l) });

        if (node->else_branch) {
            create_block(false_l);
            for (auto &s : node->else_branch->nodes) dispatch(s.get());
            emit(IROp::BR, { IROperand::create_label(end_l) });
        }
        create_block(end_l);
    }

    void visit(WhileStatementNode *node) {
        std::string cond_l = new_label("whilecond");
        std::string body_l = new_label("whilebody");
        std::string end_l = new_label("whileend");

        emit(IROp::BR, { IROperand::create_label(cond_l) });
        create_block(cond_l);
        visit_condition(node->condition.get(), body_l, end_l);

        loop_stack.push_back({ cond_l, end_l });
        create_block(body_l);
        for (auto &s : node->body->nodes) dispatch(s.get());
        emit(IROp::BR, { IROperand::create_label(cond_l) });
        loop_stack.pop_back();

        create_block(end_l);
    }

    void visit(ForStatementNode *node) {
        std::string cond_l = new_label("forcond");
        std::string body_l = new_label("forbody");
        std::string inc_l = new_label("forinc");
        std::string end_l = new_label("forend");

        if (node->initialization) dispatch(node->initialization.get());

        emit(IROp::BR, { IROperand::create_label(cond_l) });
        create_block(cond_l);
        if (node->condition)
            visit_condition(node->condition.get(), body_l, end_l);
        else
            emit(IROp::BR, { IROperand::create_label(body_l) }); // 无条件

        loop_stack.push_back({ inc_l, end_l });
        create_block(body_l);
        for (auto &s : node->body->nodes) dispatch(s.get());
        emit(IROp::BR, { IROperand::create_label(inc_l) });
        loop_stack.pop_back();

        create_block(inc_l);
        if (node->increment) dispatch_expr(node->increment.get());
        emit(IROp::BR, { IROperand::create_label(cond_l) });

        create_block(end_l);
    }

    void visit(SwitchStatementNode *node) {
        std::string end_label = new_label("switchend");
        loop_stack.push_back({ "", end_label }); // 注册 'break' 目标

        IROperand val = dispatch_expr(node->condition.get());

        std::unordered_map<int, std::string> case_targets;       // 映射: case 值 -> 目标标签
        std::unordered_map<ASTNode *, std::string> block_labels; // 映射: 块节点 -> 目标标签
        std::string default_target = end_label;                  // 默认跳转到结尾
        std::string pending_label;                               // "fall-through" 标签

        // Pass 1: 扫描 body，构建标签映射
        for (auto &stmt_ptr : node->body->nodes) {
            ASTNode *stmt = stmt_ptr.get();
            if (auto case_node = dynamic_cast<CaseStatementNode *>(stmt)) {
                if (pending_label.empty()) pending_label = new_label("caseblock");
                case_targets[case_node->case_value] = pending_label;
            } else if (dynamic_cast<DefaultStatementNode *>(stmt)) {
                if (pending_label.empty()) pending_label = new_label("casedefault");
                default_target = pending_label;
            } else if (auto block_node = dynamic_cast<CaseBlockStatementNode *>(stmt)) {
                if (!pending_label.empty()) {
                    block_labels[block_node] = pending_label;
                    pending_label.clear(); // 标签已被此块消耗
                }
                // 如果 pending_label 为空，此块将作为前一个块的穿透
            }
        }

        // Pass 2: 生成跳转表
        for (const auto &[case_val, target_label] : case_targets) {
            IROperand imm = IROperand::create_imm(case_val, IRType::get_i32());
            emit(IROp::TEST, { val, imm }); // 假设 TEST 比较 val 和 imm
            emit(IROp::BRZ, { IROperand::create_label(target_label) });
        }
        // 跳转到 default (或 end)
        emit(IROp::BR, { IROperand::create_label(default_target) });

        // Pass 3: 生成代码块
        for (auto &stmt_ptr : node->body->nodes) {
            if (auto block_node = dynamic_cast<CaseBlockStatementNode *>(stmt_ptr.get())) {
                auto it = block_labels.find(block_node);
                if (it != block_labels.end()) {
                    // 这个块是一个 case/default 的目标，创建新基本块
                    create_block(it->second);
                }
                // 访问块内的语句（在当前块中继续）
                visit(block_node);
            }
        }

        // 创建结束块
        create_block(end_label);
        loop_stack.pop_back(); // 移除 'break' 目标
    }

    void visit(CaseStatementNode *_) {
        // 在 Pass 1 中处理，这里什么都不做
    }

    void visit(DefaultStatementNode *_) {
        // 在 Pass 1 中处理，这里什么都不做
    }

    void visit(CaseBlockStatementNode *node) {
        // 访问块内的所有语句
        for (auto &s : node->body->nodes) dispatch(s.get());
    }

    void visit(ReturnStatementNode *node) {
        if (node->value)
            emit(IROp::RET, { dispatch_expr(node->value.get()) });
        else
            emit(IROp::RET, {});
        create_block(new_label("unreachable"));
    }
    void visit(BreakStatementNode *) {
        if (loop_stack.empty()) throw std::runtime_error("Break outside loop");
        emit(IROp::BR, { IROperand::create_label(loop_stack.back().second) });
        create_block(new_label("unreachable"));
    }
    void visit(ContinueStatementNode *) {
        if (loop_stack.empty()) throw std::runtime_error("Continue outside loop");
        emit(IROp::BR, { IROperand::create_label(loop_stack.back().first) });
        create_block(new_label("unreachable"));
    }

    void visit(InputStatementNode *node) {
        IROperand ptr = get_lvalue_addr(node->var.get());
        IRType *target_type = ptr.type->get_pointee_type();
        IROperand val = new_reg(target_type);
        if (target_type->is_int())
            emit(IROp::INPUT_I32, {}, val);
        else if (target_type->is_char())
            emit(IROp::INPUT_I8, {}, val);
        else
            throw std::runtime_error("Input type must be int or char");
        emit(IROp::STORE, { val, ptr });
    }

    void visit(OutputStatementNode *node) {
        IROperand val = dispatch_expr(node->var.get());
        if (val.type->is_pointer() && val.type->get_pointee_type()->is_char())
            emit(IROp::OUTPUT_STR, { val });
        else if (val.type->is_char())
            emit(IROp::OUTPUT_I8, { val });
        else
            emit(IROp::OUTPUT_I32, { val }); // 默认
    }

    IROperand visit(AssignmentNode *node) {
        IROperand rval = dispatch_expr(node->rvalue.get());
        IROperand lval_ptr = get_lvalue_addr(node->lvalue.get());
        emit(IROp::STORE, { rval, lval_ptr });
        return rval;
    }

    IROperand visit(UnaryOpNode *node) {
        if (node->op == UnaryOpKind::ADDR) {
            return get_lvalue_addr(node->operand.get());
        } else { // DEREF
            IROperand ptr = dispatch_expr(node->operand.get());
            if (!ptr.type->is_pointer()) throw std::runtime_error("Cannot dereference non-pointer");
            IROperand val = new_reg(ptr.type->get_pointee_type());
            emit(IROp::LOAD, { ptr }, val);
            return val;
        }
    }

    IROperand visit(ArrayIndexNode *node) {
        IROperand ptr = get_lvalue_addr(node);
        IROperand val = new_reg(ptr.type->get_pointee_type());
        emit(IROp::LOAD, { ptr }, val);
        return val;
    }

    IROperand visit(MemberAccessNode *node) {
        IROperand ptr = get_lvalue_addr(node);
        IROperand val = new_reg(ptr.type->get_pointee_type());
        emit(IROp::LOAD, { ptr }, val);
        return val;
    }

    IROperand visit(BinaryOpNode *node) {
        // 比较运算在 visit_condition 中处理，这里只处理算术
        IROperand lhs = dispatch_expr(node->left.get());
        IROperand rhs = dispatch_expr(node->right.get());
        IROperand res = new_reg(IRType::get_i32()); // 算术都返回 i32

        IROp op;
        switch (node->op) {
            case BinaryOpKind::ADD: op = IROp::ADD; break;
            case BinaryOpKind::SUB: op = IROp::SUB; break;
            case BinaryOpKind::MUL: op = IROp::MUL; break;
            case BinaryOpKind::DIV: op = IROp::DIV; break;
            default: throw std::runtime_error("Comparison op used in expression");
        }
        emit(op, { lhs, rhs }, res);
        return res;
    }

    IROperand visit(FunctionCallNode *node) {
        auto it = module.global_symbols.find(node->name);
        if (it == module.global_symbols.end())
            throw std::runtime_error("Function not found: " + node->name);

        IROperand func_op = it->second;
        std::vector<IROperand> args = { func_op };
        for (auto &arg_node : node->args->nodes) {
            args.push_back(dispatch_expr(arg_node.get()));
        }

        std::optional<IROperand> res = std::nullopt;
        if (!func_op.type->is_void()) {
            res = new_reg(func_op.type); // func_op.type 存储了返回类型
        }
        emit(IROp::CALL, args, res);
        return res.value_or(IROperand(IROperandType::IMM, IRType::get_void()));
    }

    IROperand visit(VariableReferenceNode *node) {
        IROperand ptr = get_symbol_ptr(node->name);
        IRType *type = ptr.type->get_pointee_type();

        // 不支持拷贝赋值 XD
        if (type->is_struct()) {
            throw std::runtime_error("Cannot use struct as r-value: " + node->name);
        }
        // 如果是 Array，退化为指向第一个元素的指针
        if (type->is_array()) {
            IRType *elem_type = type->get_array_element_type();
            IROperand res_ptr = new_reg(IRType::get_pointer(elem_type));
            // GEP: %res_ptr = gep %ptr, i32 0, i32 0
            emit(IROp::GEP,
                 { ptr, IROperand::create_imm(0, IRType::get_i32()),
                   IROperand::create_imm(0, IRType::get_i32()) },
                 res_ptr);
            return res_ptr;
        }

        // primitive or pointer
        IROperand val = new_reg(ptr.type->get_pointee_type());
        emit(IROp::LOAD, { ptr }, val);
        return val;
    }

    IROperand visit(IntegerLiteralNode *node) {
        return IROperand::create_imm(node->value, IRType::get_i32());
    }
    IROperand visit(CharacterLiteralNode *node) {
        return IROperand::create_imm(static_cast<int>(node->value), IRType::get_i8());
    }
    IROperand visit(StringLiteralNode *node) {
        std::string lbl = "@str" + std::to_string(str_cnt++);
        IRGlobalVar g(lbl, IRType::get_char_ptr());
        g.init_str = node->value;
        module.globals.push_back(g);
        return IROperand::create_global(lbl, IRType::get_char_ptr());
    }
};
