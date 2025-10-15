#pragma once

#include <string>
#include <variant>
#include <vector>

using string = std::string;

enum ASTNodeType {
    NODE_INTEGER,
    NODE_IDENTIFIER,
    NODE_BINARY_OP,
    NODE_ASSIGNMENT,
    NODE_VAR_DECLARE,
    NODE_IF,
    NODE_WHILE,
    NODE_BLOCK,
    NODE_EXPRSSION
};

enum OpType { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE };


struct ASTNode {
    typedef std::vector<ASTNode *> ASTNode_LIST;

    typedef std::variant<int, string, OpType> ASTNode_Value;


    ASTNodeType type;

    ASTNode_Value value;

    // tree struct
    ASTNode *left, *right;
    ASTNode *ex;
    ASTNode_LIST *child;

    ASTNode()
        : type(NODE_INTEGER), value(0), left(nullptr), right(nullptr), ex(nullptr), child(nullptr) {
    }

    ASTNode(ASTNodeType type, ASTNode_Value value)
        : type(type), value(value), left(nullptr), right(nullptr), ex(nullptr), child(nullptr) {}
};

typedef std::vector<ASTNode *> ASTNode_LIST;


ASTNode *create_integer_node(int value);
ASTNode *create_identifier_node(char *name);
ASTNode *create_binary_op_node(OpType op, ASTNode *left, ASTNode *right);
ASTNode *create_assignment_node(char *name, ASTNode *expr);
ASTNode *create_var_decl_node(char *name);
ASTNode *create_if_node(ASTNode *condition, ASTNode_LIST *then_block, ASTNode_LIST *else_block);
ASTNode *create_while_node(ASTNode *condition, ASTNode_LIST *body);
ASTNode *create_block_node(ASTNode_LIST *statements);
ASTNode *create_expr_stmt_node(ASTNode *expr);

ASTNode_LIST *create_block_item_list();
ASTNode_LIST *append_block_item(ASTNode_LIST *list, ASTNode *stmt);

void execute_program(ASTNode_LIST *program);
