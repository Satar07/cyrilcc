#pragma once

#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

class ASTNode;

struct ASTNode_List {
    std::vector<std::unique_ptr<ASTNode>> nodes;
};

// 全局根节点
extern std::unique_ptr<ASTNode> root;

// AST 节点基类
class ASTNode {
  public:
    virtual ~ASTNode() = default;

    virtual void print(std::ostream &os, int indent = 0) const = 0;

    // 辅助函数
  protected:
    void print_indent(std::ostream &os, int indent) const {
        for (int i = 0; i < indent; ++i) os << "  ";
    }

    void print_node_list(std::ostream &os, const ASTNode_List *list, int indent) const;
};
// --- 顶层结构 ---
class ProgramNode : public ASTNode {
  public:
    std::unique_ptr<ASTNode_List> definitions; // 顶层定义列表

    ProgramNode(ASTNode_List *defs) : definitions(defs) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// --- 类型定义 ---

// 类型
enum class TypeKind { INT, CHAR };

// 二元操作 (用于计算和比较)
enum class BinaryOpKind { ADD, SUB, MUL, DIV, LT, GT, LE, GE, EQ, NE };

// 类型说明符
class TypeSpecifierNode : public ASTNode {
  public:
    TypeKind type;

    TypeSpecifierNode(TypeKind t) : type(t) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// --- 函数定义 ---

// 函数定义
class FunctionDefinitionNode : public ASTNode {
  public:
    TypeKind return_type;
    std::string name;
    std::unique_ptr<ASTNode_List> params;
    std::unique_ptr<ASTNode_List> body;

    FunctionDefinitionNode(ASTNode *return_type_node, const char *name, ASTNode_List *params,
                           ASTNode_List *body)
        : return_type(static_cast<TypeSpecifierNode *>(return_type_node)->type), name(name),
          params(std::unique_ptr<ASTNode_List>(params)), body(std::unique_ptr<ASTNode_List>(body)) {
    }
    void print(std::ostream &os, int indent = 0) const override;
};

// 函数参数声明（每一个参数）
class ParameterDeclarationNode : public ASTNode {
  public:
    TypeKind type;
    std::string name;

    ParameterDeclarationNode(TypeKind type, const char *name) : type(type), name(name) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// -- 变量定义 ---

// 每个变量的定义
class VariableDefinitionNode : public ASTNode {
  public:
    std::string name;

    VariableDefinitionNode(const char *name) : name(name) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// 变量定义列表
class VariableDeclarationListNode : public ASTNode {
  public:
    std::unique_ptr<ASTNode> type;              // 类型 (TypeSpecifierNode)
    std::unique_ptr<ASTNode_List> declarations; // 变量列表 (VariableDefinitionNode 列表)

    VariableDeclarationListNode(ASTNode *type_node, ASTNode_List *vars)
        : type(type_node), declarations(vars) {}

    void print(std::ostream &os, int indent = 0) const override;
};

// --- 语句块 ---

// 语句基类
class StatementNode : public ASTNode {};

// input 语句
class InputStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> var; // 目标表达式

    InputStatementNode(ASTNode *var) : var(var) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// output 语句
class OutputStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> var; // 目标表达式

    OutputStatementNode(ASTNode *var) : var(var) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// if 语句
class IfStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> condition;        // 条件表达式
    std::unique_ptr<ASTNode_List> then_branch; // then 分支语句块
    std::unique_ptr<ASTNode_List> else_branch; // else 分支语句 nullptr 表示没有 else 分支

    IfStatementNode(ASTNode *condition, ASTNode_List *if_block, ASTNode_List *else_block = nullptr)
        : condition(condition), then_branch(if_block), else_branch(else_block) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// while 语句
class WhileStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> condition; // 条件表达式
    std::unique_ptr<ASTNode_List> body; // 循环体语句块

    WhileStatementNode(ASTNode *condition, ASTNode_List *body) : condition(condition), body(body) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// switch 语句
class SwitchStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> condition; // 条件表达式
    std::unique_ptr<ASTNode_List> body; // switch 语句块

    SwitchStatementNode(ASTNode *condition, ASTNode_List *body)
        : condition(condition), body(body) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// case 语句
class CaseStatementNode : public StatementNode {
  public:
    int case_value;

    CaseStatementNode(int value) : case_value(value) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// default 语句
class DefaultStatementNode : public StatementNode {
  public:
    void print(std::ostream &os, int indent = 0) const override;
};

// case 语句块
class CaseBlockStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode_List> body; // case 语句块

    CaseBlockStatementNode(ASTNode_List *body) : body(body) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// return 语句
class ReturnStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> value; // 返回值表达式

    ReturnStatementNode(ASTNode *value) : value(value) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// Break 语句
class BreakStatementNode : public StatementNode {
  public:
    void print(std::ostream &os, int indent = 0) const override;
};

// Continue 语句
class ContinueStatementNode : public StatementNode {
  public:
    void print(std::ostream &os, int indent = 0) const override;
};

// --- 表达式 ---

// 表达式基类
class ExpressionNode : public ASTNode {};

// 立即数 整数
class IntegerLiteralNode : public ExpressionNode {
  public:
    int value;

    IntegerLiteralNode(int v) : value(v) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// 立即数 字符
class CharacterLiteralNode : public ExpressionNode {
  public:
    char value;

    CharacterLiteralNode(char v) : value(v) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// 立即数：字符串
class StringLiteralNode : public ExpressionNode {
  public:
    std::string value;

    StringLiteralNode(const char *v) : value(v) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// 二元操作
class BinaryOpNode : public ExpressionNode {
  public:
    BinaryOpKind op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    BinaryOpNode(BinaryOpKind op, ASTNode *left, ASTNode *right)
        : op(op), left(left), right(right) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// 赋值表达式
class AssignmentNode : public ExpressionNode {
  public:
    std::unique_ptr<ASTNode> lvalue; // 左值 (应该是 VariableReferenceNode)
    std::unique_ptr<ASTNode> rvalue; // 右值 (任意表达式)

    AssignmentNode(ASTNode *l, ASTNode *r) : lvalue(l), rvalue(r) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// --- 变量调用 ---

// 变量引用 (在表达式中使用)
class VariableReferenceNode : public ExpressionNode {
  public:
    std::string name; // 变量名

    VariableReferenceNode(const char *n) : name(n) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// --- 函数调用 ---

// 函数调用
class FunctionCallNode : public ExpressionNode {
  public:
    std::string name;
    std::unique_ptr<ASTNode_List> args; // 参数列表

    FunctionCallNode(const char *name, ASTNode_List *args) : name(name), args(args) {}
    void print(std::ostream &os, int indent = 0) const override;
};

// -------------------------------------------------

// 顶层
void ast_setup_program(ASTNode_List *list);

// 列表操作
ASTNode_List *ast_list_create_empty();
ASTNode_List *ast_list_create(ASTNode *node);
ASTNode_List *ast_list_append(ASTNode_List *list, ASTNode *node);

// 类型
ASTNode *ast_create_type_int();
ASTNode *ast_create_type_char();

// 定义
ASTNode *
ast_create_definition_function(ASTNode *type, char *name, ASTNode_List *params, ASTNode_List *body);
ASTNode *ast_create_declaration_parameter(ASTNode *type, char *name);
ASTNode *ast_create_definition_variable(char *name);
ASTNode *ast_create_definition_variable_list(ASTNode *type, ASTNode_List *vars);

// 语句
ASTNode *ast_create_statement_input(ASTNode *expr);
ASTNode *ast_create_statement_output(ASTNode *expr);
ASTNode *ast_create_statement_if(ASTNode *cond, ASTNode_List *body);
ASTNode *
ast_create_statement_if_else(ASTNode *cond, ASTNode_List *if_body, ASTNode_List *else_body);
ASTNode *ast_create_statement_while(ASTNode *cond, ASTNode_List *body);
ASTNode *ast_create_statement_switch(ASTNode *cond, ASTNode_List *body);
ASTNode *ast_create_statement_case(int cond);
ASTNode *ast_create_statement_case(char cond); // 字符 case? 也许可行
ASTNode *ast_create_statement_default();
ASTNode *ast_create_statement_case_block(ASTNode_List *body);
ASTNode *ast_create_statement_return(ASTNode *expr);
ASTNode *ast_create_statement_break();
ASTNode *ast_create_statement_continue();

// 表达式
ASTNode *ast_create_assignment(ASTNode *var, ASTNode *expr);

ASTNode *ast_create_comparison_lt(ASTNode *l, ASTNode *r);
ASTNode *ast_create_comparison_gt(ASTNode *l, ASTNode *r);
ASTNode *ast_create_comparison_le(ASTNode *l, ASTNode *r);
ASTNode *ast_create_comparison_ge(ASTNode *l, ASTNode *r);
ASTNode *ast_create_comparison_eq(ASTNode *l, ASTNode *r);
ASTNode *ast_create_comparison_ne(ASTNode *l, ASTNode *r);

ASTNode *ast_create_calculation_add(ASTNode *l, ASTNode *r);
ASTNode *ast_create_calculation_sub(ASTNode *l, ASTNode *r);
ASTNode *ast_create_calculation_mul(ASTNode *l, ASTNode *r);
ASTNode *ast_create_calculation_div(ASTNode *l, ASTNode *r);

ASTNode *ast_create_immediate_integer(int val);
ASTNode *ast_create_immediate_character(int val);
ASTNode *ast_create_immediate_string(char *val);

// 变量调用
ASTNode *ast_create_variable_reference(char *name);

// 函数调用
ASTNode *ast_create_function_call(char *name, ASTNode_List *args);
