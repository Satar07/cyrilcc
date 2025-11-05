#pragma once

#include "ast.hpp"  // 包含 ast.hpp
#include "type.hpp" // 包含 type.hpp
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
};

// --- IR 指令集 ---
enum class IROp {
    // 终结
    RET,
    // 无条件跳转
    BR,
    // 条件跳转 (TEST/CMP + Branch)
    BRZ,  // Branch if Zero (Equal)
    BRNZ, // Branch if Not Zero (Not Equal)
    BRLT, // Branch if Less Than
    BRGT, // Branch if Greater Than
    BRLE, // Branch if Less or Equal
    BRGE, // Branch if Greater or Equal
    BREQ, // Branch if Equal (同 BRZ)
    BRNE, // Branch if Not Equal (同 BRNZ)
    // 比较 (设置标志位)
    TEST, // (在我们的实现中，TEST 被内置到条件跳转中)
    // 内存
    ALLOCA, // 分配栈空间
    LOAD,   // 从内存加载
    STORE,  // 存储到内存
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
    LABEL
};

struct IRInstruction {
    IROp op;
    std::vector<IROperand> args;
    std::optional<IROperand> result;
    IRInstruction(IROp o, std::vector<IROperand> a = {}, std::optional<IROperand> r = std::nullopt)
        : op(o), args(std::move(a)), result(std::move(r)) {}
};

// --- 基本块 ---
struct IRBasicBlock {
    std::string label;
    std::vector<IRInstruction> insts;
    IRBasicBlock(std::string l) : label(std::move(l)) {}
};

// --- 函数定义 ---
struct IRFunction {
    std::string name;
    IRType *ret_type;
    std::vector<IROperand> params;                           // 参数列表 (虚拟寄存器)
    std::vector<IRBasicBlock> blocks;                        // 基本块列表
    std::unordered_map<std::string, IROperand> symbol_table; // 局部变量表 (映射到栈指针)
    IRFunction(std::string n, IRType *rt) : name(std::move(n)), ret_type(rt) {}
};

// --- 全局变量 ---
struct IRGlobalVar {
    std::string name;
    IRType *type;
    std::string init_str; // 仅用于字符串字面量
    // (根据约束，移除其他初始化字段)
    IRGlobalVar(std::string n, IRType *t) : name(std::move(n)), type(t) {}
};

// --- 模块 (Top Level) ---
struct IRModule {
    std::vector<IRGlobalVar> globals;
    std::vector<IRFunction> functions;
    std::unordered_map<std::string, IROperand> global_symbols; // 全局符号表

    void dump(std::ostream &os) const { /* 实现省略，但需存在 */ }
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
    int vreg_cnt = 0;
    int label_cnt = 0;
    int str_cnt = 0;
    std::vector<std::pair<std::string, std::string>> loop_stack; // <continue_lbl, break_lbl>

    // --- 辅助工具 ---
    std::string new_label(const std::string &prefix = "L") {
        return prefix + "_" + std::to_string(label_cnt++);
    }
    IROperand new_reg(IRType *type) {
        return IROperand::create_reg("%" + std::to_string(vreg_cnt++), type);
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
        throw std::runtime_error("Expression is not an lvalue");
    }

    // --- 条件跳转 ---
    void
    visit_condition(ASTNode *cond, const std::string &true_label, const std::string &false_label) {
        if (auto bin_op = dynamic_cast<BinaryOpNode *>(cond)) {
            IROperand lhs = dispatch_expr(bin_op->left.get());
            IROperand rhs = dispatch_expr(bin_op->right.get());
            emit(IROp::TEST, { lhs, rhs }); // 假设 TEST 设置标志位

            IROp br_op;
            switch (bin_op->op) {
                case BinaryOpKind::LT: br_op = IROp::BRLT; break;
                case BinaryOpKind::GT: br_op = IROp::BRGT; break;
                case BinaryOpKind::LE: br_op = IROp::BRLE; break;
                case BinaryOpKind::GE: br_op = IROp::BRGE; break;
                case BinaryOpKind::EQ: br_op = IROp::BREQ; break;
                case BinaryOpKind::NE: br_op = IROp::BRNE; break;
                default: goto generic_cond; // 非比较运算
            }
            emit(br_op, { IROperand::create_label(true_label) });
            emit(IROp::BR, { IROperand::create_label(false_label) });
            return;
        }
    generic_cond:
        // 表达式作为条件 (e.g., if(x))
        IROperand val = dispatch_expr(cond);
        emit(IROp::TEST, { val, IROperand::create_imm(0, IRType::get_i32()) });
        emit(IROp::BRNZ, { IROperand::create_label(true_label) }); // 不为 0 则跳转
        emit(IROp::BR, { IROperand::create_label(false_label) });
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
        vreg_cnt = 0; // 重置虚拟寄存器

        create_block("entry");

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
        if (cur_block->insts.empty() || cur_block->insts.back().op != IROp::RET) {
            emit(IROp::RET,
                 node->return_type->is_void()
                     ? std::vector<IROperand>{}
                     : std::vector<IROperand>{ IROperand::create_imm(0, IRType::get_i32()) });
        }
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

    void visit(IfStatementNode *node) {
        std::string true_l = new_label("if_true");
        std::string false_l = node->else_branch ? new_label("if_else") : new_label("if_end");
        std::string end_l = node->else_branch ? new_label("if_end") : false_l;

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
        std::string cond_l = new_label("while_cond");
        std::string body_l = new_label("while_body");
        std::string end_l = new_label("while_end");

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
        std::string cond_l = new_label("for_cond");
        std::string body_l = new_label("for_body");
        std::string inc_l = new_label("for_inc");
        std::string end_l = new_label("for_end");

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
        std::string end_label = new_label("switch_end");
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
                if (pending_label.empty()) pending_label = new_label("case_block");
                case_targets[case_node->case_value] = pending_label;
            } else if (dynamic_cast<DefaultStatementNode *>(stmt)) {
                if (pending_label.empty()) pending_label = new_label("case_default");
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
            emit(IROp::BREQ, { IROperand::create_label(target_label) });
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
        std::string lbl = "@str_" + std::to_string(str_cnt++);
        IRGlobalVar g(lbl, IRType::get_char_ptr());
        g.init_str = node->value;
        module.globals.push_back(g);
        return IROperand::create_global(lbl, IRType::get_char_ptr());
    }
};
