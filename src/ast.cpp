#include "ast.hpp"
#include "type.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// 全局根节点
std::unique_ptr<ASTNode> root = nullptr;
// 结构体类型缓存
std::unordered_map<std::string, std::unique_ptr<IRType>> IRType::struct_cache;

// --- 接口函数 ---

// 顶层
void ast_setup_program(ASTNode_List *list) {
    root = std::make_unique<ProgramNode>(list);
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
IRType *ast_create_type_int() {
    return IRType::get_i32();
}
IRType *ast_create_type_char() {
    return IRType::get_i8();
}
IRType *ast_create_type_void() {
    return IRType::get_void();
}
IRType *ast_create_type_struct(char *name) {
    return IRType::get_struct(name);
}

// 标识符声明
ASTNode *ast_create_declarator_ident(char *name) {
    return new IdentifierDeclarationNode(name);
}
ASTNode *ast_create_declarator_ptr(ASTNode *base_type) {
    return new PointerDeclarationNode(base_type);
}
ASTNode *ast_create_declarator_array(ASTNode *base_decl, int size) {
    return new ArrayDeclarationNode(base_decl, size);
}

// 定义
ASTNode *ast_create_definition_function(IRType *type, ASTNode *ident, ASTNode_List *params,
                                        ASTNode_List *body) {
    ASTNode *node = new FunctionNode(type, ident, params, body);
    return node;
}
ASTNode *ast_create_declaration_parameter(IRType *type, ASTNode *ident) {
    return new ParameterDeclarationNode(type, ident);
}

ASTNode *ast_create_definition_variable_list(IRType *type, ASTNode_List *vars) {
    return new VariableDeclarationListNode(type, vars);
}
ASTNode *ast_create_definition_struct(char *name, ASTNode_List *fields) {
    auto node = new StructDefinitionNode(name, fields);
    return node;
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
ASTNode *ast_create_statement_for(ASTNode *init, ASTNode *cond, ASTNode *inc, ASTNode_List *body) {
    return new ForStatementNode(init, cond, inc, body);
}
ASTNode *ast_create_statement_switch(ASTNode *cond, ASTNode_List *body) {
    return new SwitchStatementNode(cond, body);
}
ASTNode *ast_create_statement_case(int cond) {
    return new CaseStatementNode(cond);
}
ASTNode *ast_create_statement_default() {
    return new DefaultStatementNode();
}
ASTNode *ast_create_statement_case_block(ASTNode_List *body) {
    return new CaseBlockStatementNode(body);
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

ASTNode *ast_create_unary_op_addr(ASTNode *expr) {
    return new UnaryOpNode(UnaryOpKind::ADDR, expr);
}
ASTNode *ast_create_unary_op_deref(ASTNode *expr) {
    return new UnaryOpNode(UnaryOpKind::DEREF, expr);
}

ASTNode *ast_create_postfix_array_index(ASTNode *array, ASTNode *index) {
    return new ArrayIndexNode(array, index);
}

ASTNode *ast_create_postfix_member_access(ASTNode *object, char *name) {
    return new MemberAccessNode(object, name);
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
