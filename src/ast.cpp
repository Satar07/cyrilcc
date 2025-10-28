#include "ast.hpp"
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// 全局根节点
std::unique_ptr<ASTNode> root = nullptr;

// 辅助函数

std::string type_kind_to_string(TypeKind type) {
    switch (type) {
        case TypeKind::INT: return "int";
        case TypeKind::CHAR: return "char";
        default: throw std::runtime_error("Unknown TypeKind");
    }
}

std::string binary_op_kind_to_string(BinaryOpKind op) {
    switch (op) {
        case BinaryOpKind::ADD: return "+";
        case BinaryOpKind::SUB: return "-";
        case BinaryOpKind::MUL: return "*";
        case BinaryOpKind::DIV: return "/";
        case BinaryOpKind::LT: return "<";
        case BinaryOpKind::GT: return ">";
        case BinaryOpKind::LE: return "<=";
        case BinaryOpKind::GE: return ">=";
        case BinaryOpKind::EQ: return "==";
        case BinaryOpKind::NE: return "!=";
        default: throw std::runtime_error("Unknown BinaryOpKind");
    }
}

// ---------------------------------
// --- ASTNode 方法实现 ---

// print 方法
void ASTNode::print_node_list(std::ostream &os, const ASTNode_List *list, int indent) const {
    if (list) {
        for (const auto &node : list->nodes) {
            if (node) {
                node->print(os, indent);
            } else {
                print_indent(os, indent);
                os << "(null node in list)" << std::endl;
            }
        }
    } else {
        print_indent(os, indent);
        os << "(null list)" << std::endl;
    }
}

void ProgramNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Program:" << std::endl;
    print_node_list(os, definitions.get(), indent + 1);
}

void TypeSpecifierNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Type: " << type_kind_to_string(type) << std::endl;
}

void FunctionDefinitionNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "FunctionDefinition: " << name << " -> " << type_kind_to_string(return_type) << std::endl;

    print_indent(os, indent + 1);
    os << "Parameters:" << std::endl;
    print_node_list(os, params.get(), indent + 2);

    print_indent(os, indent + 1);
    os << "Body:" << std::endl;
    print_node_list(os, body.get(), indent + 2);
}

void ParameterDeclarationNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Param: " << name << " (" << type_kind_to_string(type) << ")" << std::endl;
}

void VariableDefinitionNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Define var: " << name << std::endl;
}

void VariableDeclarationListNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "VarDeclarations:" << std::endl;
    type->print(os, indent + 1);
    print_node_list(os, declarations.get(), indent + 1);
}

void InputStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Input:" << std::endl;
    var->print(os, indent + 1);
}

void OutputStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Output:" << std::endl;
    var->print(os, indent + 1);
}

void IfStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "If:" << std::endl;

    print_indent(os, indent + 1);
    os << "Condition:" << std::endl;
    condition->print(os, indent + 2);

    print_indent(os, indent + 1);
    os << "Then:" << std::endl;
    print_node_list(os, then_branch.get(), indent + 2);

    if (else_branch) {
        print_indent(os, indent + 1);
        os << "Else:" << std::endl;
        print_node_list(os, else_branch.get(), indent + 2);
    }
}

void WhileStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "While:" << std::endl;

    print_indent(os, indent + 1);
    os << "Condition:" << std::endl;
    condition->print(os, indent + 2);

    print_indent(os, indent + 1);
    os << "Body:" << std::endl;
    print_node_list(os, body.get(), indent + 2);
}

void ReturnStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Return:" << std::endl;
    value->print(os, indent + 1);
}

void BreakStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Break" << std::endl;
}

void ContinueStatementNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Continue" << std::endl;
}

void IntegerLiteralNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Int: " << value << std::endl;
}

void CharacterLiteralNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Char: '" << value << "'" << std::endl;
}

void StringLiteralNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "String: \"" << value << "\"" << std::endl;
}

// 中序
void BinaryOpNode::print(std::ostream &os, int indent) const {
    left->print(os, indent + 1);
    print_indent(os, indent);
    os << binary_op_kind_to_string(op) << std::endl;
    right->print(os, indent + 1);
}

void AssignmentNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "Assign:" << std::endl;
    lvalue->print(os, indent + 1);
    rvalue->print(os, indent + 1);
}

void VariableReferenceNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "VarRef: " << name << std::endl;
}

void FunctionCallNode::print(std::ostream &os, int indent) const {
    print_indent(os, indent);
    os << "FuncCall: " << name << std::endl;
    print_indent(os, indent + 1);
    os << "Args:" << std::endl;
    print_node_list(os, args.get(), indent + 2);
}

// -------------------------------------------------
// --- 接口函数 ---

// 顶层
void ast_setup_program(ASTNode_List *list) {
    root = std::make_unique<ProgramNode>(list);
    // dbg print
    root.get()->print(std::cout);
}

// 列表操作
ASTNode_List *ast_list_create_empty() {
    return new ASTNode_List{};
}
ASTNode_List *ast_list_create(ASTNode *node) {
    auto list = new ASTNode_List{};
    if (node != nullptr) {
        list->nodes.push_back(std::unique_ptr<ASTNode>(node));
        return list;
    }
    throw std::runtime_error("Cannot create ASTNode_List with null node");
}
ASTNode_List *ast_list_append(ASTNode_List *list, ASTNode *node) {
    list->nodes.push_back(std::unique_ptr<ASTNode>(node));
    return list;
}

// 类型
ASTNode *ast_create_type_int() {
    return new TypeSpecifierNode(TypeKind::INT);
}
ASTNode *ast_create_type_char() {
    return new TypeSpecifierNode(TypeKind::CHAR);
}

// 定义
ASTNode *ast_create_definition_function(ASTNode *type, char *name, ASTNode_List *params,
                                        ASTNode_List *body) {
    ASTNode *node = new FunctionDefinitionNode(type, name, params, body);
    delete type;
    return node;
}
ASTNode *ast_create_declaration_parameter(ASTNode *type, char *name) {
    auto t = static_cast<TypeSpecifierNode *>(type)->type;
    delete type;
    return new ParameterDeclarationNode(t, name);
}
ASTNode *ast_create_definition_variable(char *name) {
    return new VariableDefinitionNode(name);
}
ASTNode *ast_create_definition_variable_list(ASTNode *type, ASTNode_List *vars) {
    return new VariableDeclarationListNode(type, vars);
}

// 语句
ASTNode *ast_create_statement_input(ASTNode *expr) {
    return new InputStatementNode(expr);
}
ASTNode *ast_create_statement_output(ASTNode *expr) {
    return new OutputStatementNode(expr);
}
ASTNode *ast_create_statement_if(ASTNode *cond, ASTNode_List *body) {
    return new IfStatementNode(cond, body);
}
ASTNode *
ast_create_statement_if_else(ASTNode *cond, ASTNode_List *if_body, ASTNode_List *else_body) {
    return new IfStatementNode(cond, if_body, else_body);
}
ASTNode *ast_create_statement_while(ASTNode *cond, ASTNode_List *body) {
    return new WhileStatementNode(cond, body);
}
ASTNode *ast_create_statement_return(ASTNode *expr) {
    return new ReturnStatementNode(expr);
}
ASTNode *ast_create_statement_break() {
    return new BreakStatementNode();
}
ASTNode *ast_create_statement_continue() {
    return new ContinueStatementNode();
}

// 表达式
ASTNode *ast_create_assignment(ASTNode *var, ASTNode *expr) {
    return new AssignmentNode(var, expr);
}

ASTNode *ast_create_comparison_lt(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::LT, l, r);
}
ASTNode *ast_create_comparison_gt(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::GT, l, r);
}
ASTNode *ast_create_comparison_le(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::LE, l, r);
}
ASTNode *ast_create_comparison_ge(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::GE, l, r);
}
ASTNode *ast_create_comparison_eq(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::EQ, l, r);
}
ASTNode *ast_create_comparison_ne(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::NE, l, r);
}

ASTNode *ast_create_calculation_add(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::ADD, l, r);
}
ASTNode *ast_create_calculation_sub(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::SUB, l, r);
}
ASTNode *ast_create_calculation_mul(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::MUL, l, r);
}
ASTNode *ast_create_calculation_div(ASTNode *l, ASTNode *r) {
    return new BinaryOpNode(BinaryOpKind::DIV, l, r);
}

ASTNode *ast_create_immediate_integer(int val) {
    return new IntegerLiteralNode(val);
}
ASTNode *ast_create_immediate_character(int val) {
    return new CharacterLiteralNode(val);
}
ASTNode *ast_create_immediate_string(char *val) {
    return new StringLiteralNode(val);
}

// 变量调用
ASTNode *ast_create_variable_reference(char *name) {
    return new VariableReferenceNode(name);
}

// 函数调用
ASTNode *ast_create_function_call(char *name, ASTNode_List *args) {
    return new FunctionCallNode(name, args);
}
