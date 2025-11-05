#pragma once

#include "type.hpp" // 包含新的类型系统
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

class ASTNode;

// --- 列表容器 ---
struct ASTNode_List {
    std::vector<std::unique_ptr<ASTNode>> nodes;
};

// 全局根节点
extern std::unique_ptr<ASTNode> root;

// --- 枚举 ---
enum class BinaryOpKind { ADD, SUB, MUL, DIV, LT, GT, LE, GE, EQ, NE };
enum class UnaryOpKind { ADDR, DEREF };

inline std::string binary_op_kind_to_string(BinaryOpKind op) {
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

// --- AST 节点基类 ---
class ASTNode {
  public:
    virtual ~ASTNode() = default;
    virtual void print(std::ostream &os, int indent = 0) const = 0;

  protected:
    void print_indent(std::ostream &os, int indent) const {
        for (int i = 0; i < indent; ++i) os << "  ";
    }
    void print_node_list(std::ostream &os, const ASTNode_List *list, int indent) const {
        if (list) {
            for (const auto &node : list->nodes) {
                if (node)
                    node->print(os, indent);
                else {
                    print_indent(os, indent);
                    os << "(null node in list)\n";
                }
            }
        } else {
            print_indent(os, indent);
            os << "(null list)\n";
        }
    }
};

// --- 顶层 ---
class ProgramNode : public ASTNode {
  public:
    std::unique_ptr<ASTNode_List> definitions;
    ProgramNode(ASTNode_List *defs) : definitions(defs) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Program:\n";
        print_node_list(os, definitions.get(), indent + 1);
    }
};

// --- 声明 (用于类型构造) ---
class DeclarationNode : public ASTNode {
  public:
    std::string name;
    DeclarationNode(std::string name) : name(name) {}
    virtual IRType *build_type(IRType *base_type) = 0;
};

class IdentifierDeclarationNode : public DeclarationNode {
  public:
    IdentifierDeclarationNode(std::string name) : DeclarationNode(name) {}
    IRType *build_type(IRType *base_type) override {
        return base_type;
    }
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "IdentDecl: " << name << "\n";
    }
};

class PointerDeclarationNode : public DeclarationNode {
  public:
    std::unique_ptr<DeclarationNode> base_declaration;
    PointerDeclarationNode(ASTNode *base_decl)
        : DeclarationNode(""), base_declaration(dynamic_cast<DeclarationNode *>(base_decl)) {
        if (!base_declaration)
            throw std::runtime_error("PointerDeclarationNode received non-DeclarationNode");
        this->name = base_declaration->name; // 名字从最内层冒泡上来
    }
    IRType *build_type(IRType *base_type) override {
        IRType *base_ir_type = base_declaration->build_type(base_type);
        return IRType::get_pointer(base_ir_type);
    }
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "PointerDecl: *\n";
        base_declaration->print(os, indent + 1);
    }
};

// --- 函数 ---
class FunctionNode : public ASTNode {
  public:
    IRType *return_type;
    std::string name;
    std::unique_ptr<ASTNode_List> params;
    std::unique_ptr<ASTNode_List> body;

    FunctionNode(IRType *type, ASTNode *ident, ASTNode_List *params, ASTNode_List *body)
        : params(params), body(body) {
        auto func_ret_decl = dynamic_cast<DeclarationNode *>(ident);
        if (!func_ret_decl) throw std::runtime_error("FunctionNode received non-DeclarationNode");
        this->name = func_ret_decl->name;
        this->return_type = func_ret_decl->build_type(type); // 组合基础类型和声明
        delete ident;                                        // 消耗掉临时的 ident 节点
    }
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Function: " << name << " -> " << return_type->to_string() << "\n";
        print_indent(os, indent + 1);
        os << "Params:\n";
        print_node_list(os, params.get(), indent + 2);
        print_indent(os, indent + 1);
        os << "Body:\n";
        print_node_list(os, body.get(), indent + 2);
    }
};

class ParameterDeclarationNode : public ASTNode {
  public:
    IRType *type; // 最终的参数类型
    std::string name;

    ParameterDeclarationNode(IRType *type, ASTNode *ident) {
        auto param_decl = dynamic_cast<DeclarationNode *>(ident);
        if (!param_decl)
            throw std::runtime_error("ParameterDeclarationNode received non-DeclarationNode");
        this->name = param_decl->name;
        this->type = param_decl->build_type(type);
        delete ident; // 消耗掉临时的 ident 节点
    }
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Param: " << name << " (" << type->to_string() << ")\n";
    }
};

// --- 变量定义 ---
class VariableDefinitionNode : public ASTNode {
  public:
    std::string name;
    IRType *type; // 最终的变量类型
    VariableDefinitionNode(const std::string &name, IRType *type) : name(name), type(type) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Define var: " << name << " (" << type->to_string() << ")\n";
    }
};

// 变量定义列表 (e.g., int a, *b;)
class VariableDeclarationListNode : public ASTNode {
  public:
    std::unique_ptr<ASTNode_List> declarations; // 包含 VariableDefinitionNode 列表

    // 构造函数：将 DeclarationNode 列表转换为 VariableDefinitionNode 列表
    VariableDeclarationListNode(IRType *base_type, ASTNode_List *decl_list)
        : declarations(new ASTNode_List{}) {
        for (auto &node_ptr : decl_list->nodes) {
            DeclarationNode *decl = dynamic_cast<DeclarationNode *>(node_ptr.release());
            if (!decl)
                throw std::runtime_error("VariableDeclarationList received non-DeclarationNode");
            std::string name = decl->name;
            IRType *full_type = decl->build_type(base_type); // 构造完整类型
            this->declarations->nodes.push_back(
                std::make_unique<VariableDefinitionNode>(name, full_type));
            delete decl; // 消耗掉临时的 decl 节点
        }
        delete decl_list; // 消耗掉临时的列表容器
    }
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "VarDeclarations:\n";
        print_node_list(os, declarations.get(), indent + 1);
    }
};

// --- 语句 ---
class StatementNode : public ASTNode {};

class InputStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> var;
    InputStatementNode(ASTNode *var) : var(var) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Input:\n";
        var->print(os, indent + 1);
    }
};

class OutputStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> var;
    OutputStatementNode(ASTNode *var) : var(var) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Output:\n";
        var->print(os, indent + 1);
    }
};

class IfStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode_List> then_branch;
    std::unique_ptr<ASTNode_List> else_branch;
    IfStatementNode(ASTNode *c, ASTNode_List *t, ASTNode_List *e = nullptr)
        : condition(c), then_branch(t), else_branch(e) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "If:\n";
        condition->print(os, indent + 1);
        print_indent(os, indent + 1);
        os << "Then:\n";
        print_node_list(os, then_branch.get(), indent + 2);
        if (else_branch) {
            print_indent(os, indent + 1);
            os << "Else:\n";
            print_node_list(os, else_branch.get(), indent + 2);
        }
    }
};

class WhileStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode_List> body;
    WhileStatementNode(ASTNode *c, ASTNode_List *b) : condition(c), body(b) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "While:\n";
        condition->print(os, indent + 1);
        print_indent(os, indent + 1);
        os << "Body:\n";
        print_node_list(os, body.get(), indent + 2);
    }
};

class ForStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> initialization, condition, increment;
    std::unique_ptr<ASTNode_List> body;
    ForStatementNode(ASTNode *i, ASTNode *c, ASTNode *inc, ASTNode_List *b)
        : initialization(i), condition(c), increment(inc), body(b) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "For:\n";
        print_indent(os, indent + 1);
        os << "Init:\n";
        if (initialization) initialization->print(os, indent + 2);
        print_indent(os, indent + 1);
        os << "Cond:\n";
        if (condition) condition->print(os, indent + 2);
        print_indent(os, indent + 1);
        os << "Inc:\n";
        if (increment) increment->print(os, indent + 2);
        print_indent(os, indent + 1);
        os << "Body:\n";
        print_node_list(os, body.get(), indent + 2);
    }
};

class SwitchStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode_List> body;
    SwitchStatementNode(ASTNode *c, ASTNode_List *b) : condition(c), body(b) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Switch:\n";
        condition->print(os, indent + 1);
        print_indent(os, indent + 1);
        os << "Body:\n";
        print_node_list(os, body.get(), indent + 2);
    }
};

class CaseStatementNode : public StatementNode {
  public:
    int case_value;
    CaseStatementNode(int v) : case_value(v) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Case: " << case_value << "\n";
    }
};

class DefaultStatementNode : public StatementNode {
  public:
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Default\n";
    }
};

class CaseBlockStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode_List> body;
    CaseBlockStatementNode(ASTNode_List *b) : body(b) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "CaseBlock:\n";
        print_node_list(os, body.get(), indent + 1);
    }
};

class ReturnStatementNode : public StatementNode {
  public:
    std::unique_ptr<ASTNode> value;
    ReturnStatementNode(ASTNode *v) : value(v) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Return:\n";
        if (value) value->print(os, indent + 1);
    }
};

class BreakStatementNode : public StatementNode {
  public:
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Break\n";
    }
};

class ContinueStatementNode : public StatementNode {
  public:
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Continue\n";
    }
};

// --- 表达式 ---
class ExpressionNode : public ASTNode {};

class IntegerLiteralNode : public ExpressionNode {
  public:
    int value;
    IntegerLiteralNode(int v) : value(v) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Int: " << value << "\n";
    }
};

class CharacterLiteralNode : public ExpressionNode {
  public:
    char value;
    CharacterLiteralNode(char v) : value(v) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Char: '" << value << "'\n";
    }
};

class StringLiteralNode : public ExpressionNode {
  private:
    std::string original_value;

  public:
    std::string value;
    StringLiteralNode(const char *v) : original_value(v) {
        if (!v) {
            return;
        }
        this->value.reserve(strlen(v));
        for (const char *p = v; *p != '\0'; ++p) {
            if (*p != '\\') {
                this->value += *p;
                continue;
            }
            ++p;
            if (*p == '\0') {
                this->value += '\\';
                break;
            }
            switch (*p) {
                case 'n': this->value += '\n'; break;  // 换行
                case 't': this->value += '\t'; break;  // 水平制表
                case 'r': this->value += '\r'; break;  // 回车
                case 'b': this->value += '\b'; break;  // 退格
                case 'f': this->value += '\f'; break;  // 换页
                case 'v': this->value += '\v'; break;  // 垂直制表
                case 'a': this->value += '\a'; break;  // 响铃
                case '\\': this->value += '\\'; break; // 反斜杠
                case '\"': this->value += '\"'; break; // 双引号
                case '\'': this->value += '\''; break; // 单引号
                case '0': this->value += '\0'; break;  // 空字符
                default: this->value += *p; break;     // 未知的转义序列 (例如 "\z")
            }
        }
    }
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "String: \"" << original_value << "\"\n";
    }
};

class BinaryOpNode : public ExpressionNode {
  public:
    BinaryOpKind op;
    std::unique_ptr<ASTNode> left, right;
    BinaryOpNode(BinaryOpKind o, ASTNode *l, ASTNode *r) : op(o), left(l), right(r) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "BinOp: " << binary_op_kind_to_string(op) << "\n";
        left->print(os, indent + 1);
        right->print(os, indent + 1);
    }
};

class UnaryOpNode : public ExpressionNode {
  public:
    UnaryOpKind op;
    std::unique_ptr<ASTNode> operand;
    UnaryOpNode(UnaryOpKind o, ASTNode *opd) : op(o), operand(opd) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "UnaryOp: " << (op == UnaryOpKind::ADDR ? "&" : "*") << "\n";
        operand->print(os, indent + 1);
    }
};

class AssignmentNode : public ExpressionNode {
  public:
    std::unique_ptr<ASTNode> lvalue, rvalue;
    AssignmentNode(ASTNode *l, ASTNode *r) : lvalue(l), rvalue(r) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "Assign:\n";
        lvalue->print(os, indent + 1);
        rvalue->print(os, indent + 1);
    }
};

class VariableReferenceNode : public ExpressionNode {
  public:
    std::string name;
    VariableReferenceNode(const char *n) : name(n) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "VarRef: " << name << "\n";
    }
};

class FunctionCallNode : public ExpressionNode {
  public:
    std::string name;
    std::unique_ptr<ASTNode_List> args;
    FunctionCallNode(const char *n, ASTNode_List *a) : name(n), args(a) {}
    void print(std::ostream &os, int indent = 0) const override {
        print_indent(os, indent);
        os << "FuncCall: " << name << "\n";
        print_indent(os, indent + 1);
        os << "Args:\n";
        print_node_list(os, args.get(), indent + 2);
    }
};

// --- C 接口声明 (供 yacc/bison 使用) ---
extern "C" {
void ast_setup_program(ASTNode_List *list);
ASTNode_List *ast_list_create_empty();
ASTNode_List *ast_list_create(ASTNode *node);
ASTNode_List *ast_list_append(ASTNode_List *list, ASTNode *node);

IRType *ast_create_type_int();
IRType *ast_create_type_char();
IRType *ast_create_type_void();

ASTNode *ast_create_declarator_ident(char *name);
ASTNode *ast_create_declarator_ptr(ASTNode *base_type);

ASTNode *ast_create_definition_function(IRType *type, ASTNode *ident, ASTNode_List *params,
                                        ASTNode_List *body);
ASTNode *ast_create_declaration_parameter(IRType *type, ASTNode *name);

ASTNode *ast_create_definition_variable_list(IRType *type, ASTNode_List *vars);

ASTNode *ast_create_statement_input(ASTNode *expr);
ASTNode *ast_create_statement_output(ASTNode *expr);
ASTNode *ast_create_statement_if(ASTNode *cond, ASTNode_List *body);
ASTNode *
ast_create_statement_if_else(ASTNode *cond, ASTNode_List *if_body, ASTNode_List *else_body);
ASTNode *ast_create_statement_while(ASTNode *cond, ASTNode_List *body);
ASTNode *ast_create_statement_for(ASTNode *init, ASTNode *cond, ASTNode *inc, ASTNode_List *body);
ASTNode *ast_create_statement_switch(ASTNode *cond, ASTNode_List *body);
ASTNode *ast_create_statement_case(int cond);
ASTNode *ast_create_statement_default();
ASTNode *ast_create_statement_case_block(ASTNode_List *body);
ASTNode *ast_create_statement_return(ASTNode *expr);
ASTNode *ast_create_statement_break();
ASTNode *ast_create_statement_continue();

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

ASTNode *ast_create_unary_op_addr(ASTNode *expr);
ASTNode *ast_create_unary_op_deref(ASTNode *expr);

ASTNode *ast_create_immediate_integer(int val);
ASTNode *ast_create_immediate_character(int val);
ASTNode *ast_create_immediate_string(char *val);

ASTNode *ast_create_variable_reference(char *name);
ASTNode *ast_create_function_call(char *name, ASTNode_List *args);
}
