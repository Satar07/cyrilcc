#include "ast.hpp"
#include "symbol.hpp"
#include <cassert>
#include <variant>

ASTNode *create_integer_node(int value) {
    ASTNode *new_node = new ASTNode(NODE_INTEGER, value);
    return new_node;
}

ASTNode *create_identifier_node(char *name) {
    ASTNode *new_node = new ASTNode(NODE_IDENTIFIER, name);
    return new_node;
}

ASTNode *create_binary_op_node(OpType op, ASTNode *left, ASTNode *right) {
    ASTNode *new_node = new ASTNode(NODE_BINARY_OP, op);
    new_node->left = left;
    new_node->right = right;
    return new_node;
}

ASTNode *create_assignment_node(char *name, ASTNode *expr) {
    ASTNode *new_node = new ASTNode(NODE_ASSIGNMENT, name);
    new_node->left = expr; // only left
    return new_node;
}
ASTNode *create_var_decl_node(char *name) {
    ASTNode *new_node = new ASTNode(NODE_VAR_DECLARE, name);
    return new_node;
}
ASTNode *create_if_node(ASTNode *condition, ASTNode_LIST *then_block, ASTNode_LIST *else_block) {
    ASTNode *new_node = new ASTNode(NODE_IF, 0);
    new_node->left = condition;
    new_node->right = create_block_node(then_block);
    if (else_block) {
        new_node->ex = create_block_node(else_block);
    }
    return new_node;
}
ASTNode *create_while_node(ASTNode *condition, ASTNode_LIST *body) {
    ASTNode *new_node = new ASTNode(NODE_WHILE, 0);
    new_node->left = condition;
    new_node->right = create_block_node(body);
    return new_node;
}

ASTNode *create_block_node(ASTNode_LIST *statements) {
    ASTNode *new_node = new ASTNode(NODE_BLOCK, 0);
    new_node->child = statements;
    return new_node;
}
ASTNode *create_expr_stmt_node(ASTNode *expr) {
    ASTNode *new_node = new ASTNode(NODE_EXPRSSION, 0);
    new_node->left = expr;
    return new_node;
}

ASTNode_LIST *create_block_item_list() {
    return new ASTNode_LIST();
}
ASTNode_LIST *append_block_item(ASTNode_LIST *list, ASTNode *stmt) {
    assert(list != nullptr and stmt != nullptr);
    list->push_back(stmt);
    return list;
}

// interpreter
using std::get, std::variant;

void yyerror(const char *msg);
void execute_block(ASTNode_LIST *block);
void execute_block_item(ASTNode *stmt);
variant<int, bool> evaluate_expression(ASTNode *expr);

void execute_program(ASTNode_LIST *program) {
    execute_block(program);
}

void execute_block(ASTNode_LIST *block) {
    if (block == nullptr) return;
    for (ASTNode *stmt : *block) {
        execute_block_item(stmt);
    }
}

void execute_block_item(ASTNode *item) {
    assert(item->type == NODE_EXPRSSION || item->type == NODE_VAR_DECLARE ||
           item->type == NODE_IF || item->type == NODE_WHILE);
    switch (item->type) {
        case NODE_EXPRSSION: {
            evaluate_expression(item->left);
            break;
        }
        case NODE_VAR_DECLARE: {
            char *var_name = get<string>(item->value).data();
            declare_symbol(var_name);
            break;
        }
        case NODE_IF: {
            bool cond = get<bool>(evaluate_expression(item->left));
            if (cond) {
                execute_block(item->right->child);
            } else if (item->ex) {
                execute_block(item->ex->child);
            }
            break;
        }
        case NODE_WHILE: {
            while (get<bool>(evaluate_expression(item->left))) {
                execute_block(item->right->child);
            }
            break;
        }
        default:
            assert(false);
    }
}

variant<int, bool> evaluate_expression(ASTNode *expr) {
    assert(expr->type == NODE_INTEGER || expr->type == NODE_IDENTIFIER ||
           expr->type == NODE_BINARY_OP || expr->type == NODE_ASSIGNMENT);
    switch (expr->type) {
        case NODE_INTEGER: {
            return get<int>(expr->value);
        }
        case NODE_IDENTIFIER: {
            char *var_name = get<string>(expr->value).data();
            return load_symbol(var_name).num;
        }
        case NODE_ASSIGNMENT: {
            const char *var_name = get<string>(expr->value).data();
            int val = get<int>(evaluate_expression(expr->left));
            SymbolValue value;
            value.num = val;
            store_symbol(var_name, value);
            return val;
        }
        case NODE_BINARY_OP: {
            int left_val = get<int>(evaluate_expression(expr->left));
            int right_val = get<int>(evaluate_expression(expr->right));
            OpType op = get<OpType>(expr->value);
            switch (op) {
                case OP_ADD: return left_val + right_val;
                case OP_SUB: return left_val - right_val;
                case OP_MUL: return left_val * right_val;
                case OP_DIV:
                    if (right_val == 0) {
                        yyerror("Division by zero");
                        return 0;
                    }
                    return left_val / right_val;
                case OP_LT: return left_val < right_val;
                case OP_GT: return left_val > right_val;
                case OP_LE: return left_val <= right_val;
                case OP_GE: return left_val >= right_val;
                case OP_EQ: return left_val == right_val;
                case OP_NE: return left_val != right_val;
                default: yyerror("Unknown binary operator"); return 0;
            }
        }
        default:
            assert(false);
            return 0;
    }
}