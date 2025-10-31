#include "ir.hpp"
#include "ast.hpp" // 确保包含了 ast.hpp
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// --- 辅助方法实现 ---

IRType IRGenerator::convert_ast_type(TypeKind type) {
    switch (type) {
        case TypeKind::INT: return IRType::I32;
        case TypeKind::CHAR: return IRType::I8;
        default: throw std::runtime_error("Unknown AST TypeKind");
    }
}

// 创建一个新的 SSA 寄存器 (用于值)
IROperand IRGenerator::new_reg(IRType type) {
    std::string reg_name = "%" + std::to_string(reg_count++);
    return IROperand(reg_name, type);
}

// 创建一个新的 SSA 寄存器 (用于指针)
IROperand IRGenerator::new_reg_ptr(IRType pointee_type) {
    std::string reg_name = "%" + std::to_string(reg_count++);
    return IROperand(reg_name, IRType::PTR, pointee_type);
}

std::string IRGenerator::new_label_name() {
    return "%L" + std::to_string(label_count++);
}

void IRGenerator::start_function(std::string name, IRType ret_type) {
    module.functions.emplace_back(name, ret_type);
    current_function = &module.functions.back();
    reg_count = 0; // 每个函数的寄存器重新从 0 开始
}

void IRGenerator::end_function() {
    current_function = nullptr;
    current_block = nullptr;
}

// 创建一个新的基本块，后根据可选名称创建标签，设置当前块为此块
IRBasicBlock *IRGenerator::create_block_and_set(std::string name) {
    if (name.empty()) {
        name = new_label_name();
    }
    // 创建空 block 在最后
    current_function->blocks.emplace_back(name);
    // 插入 LABEL 伪指令
    current_function->blocks.back().instructions.push_back(
        IRInstruction(IROp::LABEL, { IROperand(name, IRType::LABEL) }));
    return current_block = &current_function->blocks.back();
}

void IRGenerator::emit(IRInstruction instruction) {
    if (!current_block) {
        throw std::runtime_error("Cannot emit instruction: no current basic block");
    }
    current_block->instructions.push_back(instruction);
}

// 查找变量的指针
IROperand IRGenerator::get_var(const std::string &name) {
    // 1. 查找局部变量 (在当前函数)
    if (current_function) {
        auto it = current_function->symbol_table.find(name);
        if (it != current_function->symbol_table.end()) {
            return it->second;
        }
    }
    // 2. 查找全局变量
    auto it = module.symbol_table.find(name);
    if (it != module.symbol_table.end()) {
        return it->second;
    }
    throw std::runtime_error("Unknown variable referenced: " + name);
}

// --- Visitor 实现 ---

void IRGenerator::visit(ProgramNode *node) {
    // 第一遍：处理全局变量和函数声明 (为了支持函数调用)
    for (const auto &def : node->definitions->nodes) {
        if (auto func_def = dynamic_cast<FunctionDefinitionNode *>(def.get())) {
            // 将函数名注册到全局符号表
            std::string func_name = "@" + func_def->name;
            IRType ret_type = convert_ast_type(func_def->return_type);
            // 使用返回类型来存储函数类型信息
            module.symbol_table[func_def->name] = IROperand(func_name, ret_type);
        } else if (auto var_list = dynamic_cast<VariableDeclarationListNode *>(def.get())) {
            // 全局变量
            IRType type =
                convert_ast_type(static_cast<TypeSpecifierNode *>(var_list->type.get())->type);
            for (const auto &var : var_list->declarations->nodes) {
                auto var_def = static_cast<VariableDefinitionNode *>(var.get());
                std::string global_name = "@" + var_def->name;

                module.global_vars.emplace_back(global_name, type);
                // 全局变量在符号表中也是一个指针
                module.symbol_table[var_def->name] = IROperand(global_name, IRType::PTR, type);
            }
        }
    }

    // 第二遍：访问函数定义
    for (const auto &def : node->definitions->nodes) {
        if (auto func_def = dynamic_cast<FunctionDefinitionNode *>(def.get())) {
            visit(func_def);
        }
    }
}

void IRGenerator::visit(FunctionDefinitionNode *node) {
    IRType ret_type = convert_ast_type(node->return_type);
    std::string func_name = "@" + node->name;

    start_function(func_name, ret_type);

    // 创建入口块
    create_block_and_set();

    // --- 处理参数 (alloca/store) ---
    for (const auto &param : node->params->nodes) {
        auto param_decl = static_cast<ParameterDeclarationNode *>(param.get());
        IRType param_type = convert_ast_type(param_decl->type);

        // 1. 创建一个操作数来代表传入的参数值
        IROperand param_value = new_reg(param_type);
        current_function->params.push_back(param_value);

        // 2. ALLOCA: 为参数在栈上分配空间
        IROperand param_ptr = new_reg_ptr(param_type);
        emit(IRInstruction(IROp::ALLOCA, {}, param_ptr));

        // 3. STORE: 将传入的参数值存入分配的栈空间
        emit(IRInstruction(IROp::STORE, { param_value, param_ptr }));

        // 4. 将参数 *指针* 存入局部符号表
        current_function->symbol_table[param_decl->name] = param_ptr;
    }

    // --- 访问函数体 ---
    for (const auto &stmt : node->body->nodes) {
        dispatch(stmt.get());
    }

    // --- 确保函数有 RET ---
    // (如果最后一个块没有终结指令)
    if ((current_block && current_block->instructions.empty()) ||
        (current_block && current_block->instructions.back().op != IROp::RET &&
         current_block->instructions.back().op != IROp::BR)) {
        // if (ret_type == IRType::VOID) {
        emit(IRInstruction(IROp::RET, { IROperand{ 0, IRType::I32, IRType::VOID } }));
        // }
    }

    end_function();
}

void IRGenerator::visit(VariableDeclarationListNode *node) {
    // 注意: 全局变量已在 ProgramNode 中处理。这里只处理局部变量。
    if (!current_function) return;

    IRType type = convert_ast_type(static_cast<TypeSpecifierNode *>(node->type.get())->type);

    for (const auto &var : node->declarations->nodes) {
        auto var_def = static_cast<VariableDefinitionNode *>(var.get());

        // 1. ALLOCA: 为局部变量分配栈空间
        IROperand var_ptr = new_reg_ptr(type);
        emit(IRInstruction(IROp::ALLOCA, {}, var_ptr));

        // 2. 将变量指针存入局部符号表
        current_function->symbol_table[var_def->name] = var_ptr;
    }
}

// --- 表达式 ---

IROperand IRGenerator::visit(IntegerLiteralNode *node) {
    return IROperand(node->value, IRType::I32);
}

IROperand IRGenerator::visit(CharacterLiteralNode *node) {
    return IROperand(node->value, IRType::I8);
}

IROperand IRGenerator::visit(StringLiteralNode *node) {
    // 查找是否已有该字符串
    auto it = string_literal_map.find(node->value);
    if (it != string_literal_map.end()) {
        return IROperand(it->second, IRType::PTR, IRType::I8);
    }

    // 创建新的全局字符串
    std::string str_name = "'" + std::to_string(str_literal_count++);
    string_literal_map[node->value] = str_name;

    // 添加到模块的全局变量
    module.global_vars.emplace_back(str_name, IRType::PTR, node->value);

    return IROperand(str_name, IRType::PTR, IRType::I8);
}

IROperand IRGenerator::visit(VariableReferenceNode *node) {
    // 访问变量引用 (R-value) -> LOAD

    // 1. 找到变量的 *指针*
    IROperand var_ptr = get_var(node->name);

    // 2. LOAD: 从指针加载值
    IROperand result_reg = new_reg(var_ptr.pointee_type);
    emit(IRInstruction(IROp::LOAD, { var_ptr }, result_reg));

    return result_reg;
}

IROperand IRGenerator::visit(AssignmentNode *node) {
    // 1. 访问右侧表达式 (R-value)
    IROperand r_value = dispatch_expr(node->rvalue.get());

    // 2. 获取左侧变量的 *指针* (L-value)
    // (假设左侧总是 VariableReferenceNode)
    auto var_ref = dynamic_cast<VariableReferenceNode *>(node->lvalue.get());
    if (!var_ref) {
        throw std::runtime_error("L-value of assignment must be a variable");
    }
    IROperand l_ptr = get_var(var_ref->name);

    // 3. STORE: 将 R-value 存入 L-value 的指针
    emit(IRInstruction(IROp::STORE, { r_value, l_ptr }));

    // 赋值表达式本身也返回 R-value
    return r_value;
}

IROperand IRGenerator::visit(BinaryOpNode *node) {
    // 1. 访问左右操作数
    IROperand left_val = dispatch_expr(node->left.get());
    IROperand right_val = dispatch_expr(node->right.get());

    IROp op;
    switch (node->op) {
        // --- 算术操作 ---
        case BinaryOpKind::ADD: op = IROp::ADD; break;
        case BinaryOpKind::SUB: op = IROp::SUB; break;
        case BinaryOpKind::MUL: op = IROp::MUL; break;
        case BinaryOpKind::DIV: op = IROp::DIV; break;

        // --- 比较操作 ---
        // 按照我们的设计, 比较操作不应在此处作为表达式被求值。
        // 它们应该只在 if/while 的 visit_condition 中被处理。
        case BinaryOpKind::LT:
        case BinaryOpKind::GT:
        case BinaryOpKind::LE:
        case BinaryOpKind::GE:
        case BinaryOpKind::EQ:
        case BinaryOpKind::NE:
            throw std::runtime_error(
                "Comparison operators can only be used in if/while conditions");

        default: throw std::runtime_error("Unknown BinaryOpKind");
    }

    // 2. 创建结果寄存器
    // (假设所有算术操作返回 I32)
    IROperand result_reg = new_reg(IRType::I32);

    // 3. 发出指令
    emit(IRInstruction(op, { left_val, right_val }, result_reg));

    return result_reg;
}

IROperand IRGenerator::visit(FunctionCallNode *node) {
    // 1. 查找函数定义
    auto it = module.symbol_table.find(node->name);
    if (it == module.symbol_table.end()) {
        throw std::runtime_error("Call to undefined function: " + node->name);
    }
    IROperand func_op = it->second;
    IRType ret_type = func_op.type; // 我们在 ProgramNode 中存储了返回类型

    // 2. 访问所有参数
    std::vector<IROperand> args;
    // 第一个操作数是函数名
    args.push_back(func_op);

    for (const auto &arg : node->args->nodes) {
        args.push_back(dispatch_expr(arg.get()));
    }

    // 3. 创建结果寄存器 (如果函数有返回值)
    IROperand result_reg;
    if (ret_type != IRType::VOID) {
        result_reg = new_reg(ret_type);
    }

    // 4. 发出 CALL 指令
    emit(IRInstruction(IROp::CALL, args, result_reg));

    return result_reg;
}

// --- 语句 ---

void IRGenerator::visit(ReturnStatementNode *node) {
    IROperand ret_val = dispatch_expr(node->value.get());
    emit(IRInstruction(IROp::RET, { ret_val }));
}

void IRGenerator::visit(InputStatementNode *node) {
    // input(var) -> 1. call input, 2. store result to var

    // 1. 获取 var 的指针
    auto var_ref = dynamic_cast<VariableReferenceNode *>(node->var.get());
    if (!var_ref) {
        throw std::runtime_error("Input target must be a variable");
    }
    IROperand l_ptr = get_var(var_ref->name);

    // 2. 确定 input 类型
    IROp input_op;
    IRType input_type;
    if (l_ptr.pointee_type == IRType::I32) {
        input_op = IROp::INPUT_INT;
        input_type = IRType::I32;
    } else if (l_ptr.pointee_type == IRType::I8) {
        input_op = IROp::INPUT_CHAR;
        input_type = IRType::I8;
    } else {
        throw std::runtime_error("Input type must be int or char");
    }

    // 3. 发出 INPUT 指令 (结果存入新寄存器)
    IROperand result_reg = new_reg(input_type);
    emit(IRInstruction(input_op, {}, result_reg));

    // 4. STORE 结果
    emit(IRInstruction(IROp::STORE, { result_reg, l_ptr }));
}

void IRGenerator::visit(OutputStatementNode *node) {
    // 1. 访问要输出的表达式
    IROperand val = dispatch_expr(node->var.get());

    // 2. 确定 output 类型
    IROp output_op;
    if (val.type == IRType::I32) {
        output_op = IROp::OUTPUT_INT;
    } else if (val.type == IRType::I8) {
        output_op = IROp::OUTPUT_CHAR;
    } else if (val.type == IRType::PTR && val.pointee_type == IRType::I8) {
        // 识别为字符串 (char*)
        output_op = IROp::OUTPUT_STRING;
    } else {
        throw std::runtime_error("Output type must be int, char, or string");
    }

    // 3. 发出 OUTPUT 指令
    emit(IRInstruction(output_op, { val }));
}

// --- 控制流 ---

// 处理 if/while 的条件
void IRGenerator::visit_condition(ASTNode *cond, const std::string &true_label,
                                  const std::string &false_label) {

    IROperand true_target = IROperand(true_label, IRType::LABEL);
    IROperand false_target = IROperand(false_label, IRType::LABEL);
    bool need_revert = false;

    // 情况 1: 条件是一个二元比较 (e.g., a < b)
    if (auto bin_op = dynamic_cast<BinaryOpNode *>(cond)) {
        IROperand left_val = dispatch_expr(bin_op->left.get());
        IROperand right_val = dispatch_expr(bin_op->right.get());

        // 1. 发出 TEST 指令
        emit(IRInstruction(IROp::TEST, { left_val, right_val }));

        // 2. 根据比较类型发出条件跳转
        switch (bin_op->op) {
            case BinaryOpKind::LT: emit(IRInstruction(IROp::BRLT, { true_target })); break;
            case BinaryOpKind::GT: emit(IRInstruction(IROp::BRGT, { true_target })); break;
            case BinaryOpKind::LE: // (a <= b) -> not (a > b)
                emit(IRInstruction(IROp::BRGT, { false_target }));
                need_revert = true;
                break;
            case BinaryOpKind::GE: // (a >= b) -> not (a < b)
                emit(IRInstruction(IROp::BRLT, { false_target }));
                need_revert = true;
                break;
            case BinaryOpKind::EQ: // (a == b) -> Branch if Zero
                emit(IRInstruction(IROp::BRZ, { true_target }));
                break;
            case BinaryOpKind::NE: // (a != b) -> Branch if Not Zero
                emit(IRInstruction(IROp::BRZ, { false_target }));
                need_revert = true;
                break;
            default: throw std::runtime_error("Non-comparison op in condition");
        }
    }
    // 情况 2: 条件是一个值 (e.g., if(x), while(1))
    else {
        IROperand cond_val = dispatch_expr(cond);
        // 假设与 0 比较
        IROperand zero = IROperand(0, IRType::I32);

        // 1. 发出 TEST 指令
        emit(IRInstruction(IROp::TEST, { cond_val, zero }));

        // 2. if(x) -> if(x != 0) -> BRNZ
        emit(IRInstruction(IROp::BRZ, { false_target }));
        need_revert = true;
    }

    // 另一个分支
    create_block_and_set();
    if (need_revert) {
        emit(IRInstruction(IROp::BR, { true_target }));
    } else {
        emit(IRInstruction(IROp::BR, { false_target }));
    }
}

void IRGenerator::visit(IfStatementNode *node) {
    // 1. 创建标签
    std::string then_label = new_label_name();
    std::string end_label = new_label_name();
    std::string else_label = node->else_branch ? new_label_name() : end_label;

    // 2. 访问条件
    visit_condition(node->condition.get(), then_label, else_label);

    // 3. 'Then' 块
    create_block_and_set(then_label);
    for (const auto &stmt : node->then_branch->nodes) {
        dispatch(stmt.get());
    }
    // 'Then' 块结束后无条件跳转到 'End'
    emit(IRInstruction(IROp::BR, { IROperand(end_label, IRType::LABEL) }));

    // 4. 'Else' 块 (如果存在)
    if (node->else_branch) {
        create_block_and_set(else_label);
        for (const auto &stmt : node->else_branch->nodes) {
            dispatch(stmt.get());
        }
        // 'Else' 块结束后无条件跳转到 'End'
        emit(IRInstruction(IROp::BR, { IROperand(end_label, IRType::LABEL) }));
    }

    // 5. 'End' 块
    create_block_and_set(end_label);
}

void IRGenerator::visit(WhileStatementNode *node) {
    // 1. 创建标签
    std::string cond_label = new_label_name();
    std::string body_label = new_label_name();
    std::string end_label = new_label_name();

    // 2. 注册 break/continue 标签
    loop_start_labels.push_back(cond_label);
    loop_switch_end_labels.push_back(end_label);

    // 3. 无条件跳转到 'Cond' 块
    emit(IRInstruction(IROp::BR, { IROperand(cond_label, IRType::LABEL) }));

    // 4. 'Cond' 块
    create_block_and_set(cond_label);
    visit_condition(node->condition.get(), body_label, end_label);

    // 5. 'Body' 块
    create_block_and_set(body_label);
    for (const auto &stmt : node->body->nodes) {
        dispatch(stmt.get());
    }
    // 'Body' 块结束后无条件跳转回 'Cond'
    emit(IRInstruction(IROp::BR, { IROperand(cond_label, IRType::LABEL) }));

    // 6. 'End' 块
    create_block_and_set(end_label);

    // 7. 移除 break/continue 标签
    loop_start_labels.pop_back();
    loop_switch_end_labels.pop_back();
}

void IRGenerator::visit(SwitchStatementNode *node) {
    // 1. 创建结束标签并注册
    auto end_label = new_label_name();
    loop_switch_end_labels.push_back(end_label);

    // 2. 访问 switch 条件表达式
    IROperand switch_val = dispatch_expr(node->condition.get());

    // 3. 预先遍历 case 块，创建标签并生成比较跳转
    int block_cnt = 0;
    auto case_label_map = std::map<int, int>{}; // case value -> label index
    auto case_block_arr = std::vector<std::string>{};
    int default_block_index = -1;

    for (const auto &stmt : node->body.get()->nodes) {
        if (auto cast_stmt = dynamic_cast<CaseStatementNode *>(stmt.get())) {
            case_label_map.insert({ cast_stmt->case_value, block_cnt }); // 跳转到最近的
        }
        if (const auto &block_stmt = dynamic_cast<CaseBlockStatementNode *>(stmt.get())) {
            block_cnt++;
            case_block_arr.push_back(new_label_name());
        }
        if (const auto &default_stmt = dynamic_cast<DefaultStatementNode *>(stmt.get()) and
                                       default_block_index == -1) {
            default_block_index = block_cnt;
        }
    }

    // 4. 生成比较跳转
    bool first_case = true;

    auto create_block_when_not_fir = [&]() {
        if (first_case) {
            first_case = false;
        } else {
            create_block_and_set();
        }
    };
    for (const auto &[case_value, label_index] : case_label_map) {
        create_block_when_not_fir();
        // 生成 TEST 指令
        IROperand case_const = IROperand(case_value, IRType::I32);
        emit(IRInstruction(IROp::TEST, { switch_val, case_const }));
        // 生成 BRZ 指令跳转到对应 case 块
        emit(IRInstruction(IROp::BRZ, { IROperand(case_block_arr[label_index], IRType::LABEL) }));
    }
    if (default_block_index != -1) {
        create_block_when_not_fir();
        // 跳转到 default 块
        emit(IRInstruction(IROp::BR,
                           { IROperand(case_block_arr[default_block_index], IRType::LABEL) }));
    } else {
        create_block_when_not_fir();
        // 没有匹配的 case，也没有 default，跳转到 end
        emit(IRInstruction(IROp::BR, { IROperand(end_label, IRType::LABEL) }));
    }

    // 5. case 实际执行块
    auto now_block_cnt = 0;
    for (const auto &stmt : node->body.get()->nodes) {
        if (const auto &block_stmt = dynamic_cast<CaseBlockStatementNode *>(stmt.get())) {
            // 进入对应的 case 块
            create_block_and_set(case_block_arr[now_block_cnt]);
            for (const auto &inner_stmt : block_stmt->body.get()->nodes) {
                dispatch(inner_stmt.get());
            }
            now_block_cnt++;
        }
    }

    // 6. 'End' 块
    create_block_and_set(end_label);
    // 7. 移除 switch 结束标签
    loop_switch_end_labels.pop_back();
}

void IRGenerator::visit(BreakStatementNode *) {
    if (loop_switch_end_labels.empty()) {
        throw std::runtime_error("Break statement outside of loop");
    }
    emit(IRInstruction(IROp::BR, { IROperand(loop_switch_end_labels.back(), IRType::LABEL) }));
}

void IRGenerator::visit(ContinueStatementNode *) {
    if (loop_start_labels.empty()) {
        throw std::runtime_error("Continue statement outside of loop");
    }
    emit(IRInstruction(IROp::BR, { IROperand(loop_start_labels.back(), IRType::LABEL) }));
}
