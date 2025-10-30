#pragma once

#include "ast.hpp"
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// --- IR 类型系统 ---
enum class IRType { VOID, I32, I8, PTR, LABEL };
// 均为寄存器中的形式

// --- IR 操作数 ---
// 操作数可以是寄存器、常量或全局/函数名。
struct IROperand {
    // Value:
    // int: 整数常量 (e.g., 5)
    // char: 字符常量 (e.g., 'a')
    // std::string: 寄存器 (e.g., "%0"), 标签 (e.g., "%L0"), 全局变量/函数 (e.g., "@var")
    using Value = std::variant<int, char, std::string>;

    Value value;
    IRType type;
    IRType pointee_type = IRType::VOID; // 如果 type 是 PTR, 这是它指向的类型

    IROperand() : type(IRType::VOID) {}
    IROperand(Value v, IRType t, IRType p_type = IRType::VOID)
        : value(v), type(t), pointee_type(p_type) {}
};

// --- IR 指令 ---
enum class IROp {
    // 终结指令
    RET, // 从函数返回
    BR,  // 无条件跳转
    // 比较操作
    TEST,
    // 比较跳转
    BRZ,  // Branch if Zero (Equal)
    BRLT, // Branch if Less Than
    BRGT, // Branch if Greater Than

    // 内存操作
    ALLOCA, // 在栈上分配内存
    LOAD,   // 从内存加载值
    STORE,  // 将值存储到内存

    // 赋值操作（可优化）
    MOV, // A <- B

    // 二元操作
    ADD,
    SUB,
    MUL,
    DIV,

    // 函数调用
    CALL,

    // 输入/输出操作
    INPUT_INT,     // 输入整数 (ITI)
    INPUT_CHAR,    // 输入字符 (ITC)
    OUTPUT_INT,    // 输出整数 (OTI)
    OUTPUT_CHAR,   // 输出字符 (OTC)
    OUTPUT_STRING, // 输出字符串 (OTS)

    // 其他
    LABEL, // 用于标签的伪指令
};

struct IRInstruction {
    IROp op;
    std::vector<IROperand> operands;
    IROperand result; // 指令结果的存储位置 (如果有)

    IRInstruction(IROp op, std::vector<IROperand> operands = {}, IROperand result = {})
        : op(op), operands(std::move(operands)), result(std::move(result)) {}
};

// --- 基本块 ---
struct IRBasicBlock {
    std::string label;
    std::vector<IRInstruction> instructions;

    IRBasicBlock(std::string label) : label(std::move(label)) {}
};

// --- 函数 ---
struct IRFunction {
    std::string name;
    IRType return_type;
    std::vector<IROperand> params;
    std::vector<IRBasicBlock> blocks;

    // 局部符号表 (存储变量名到其 *指针* 操作数 (来自 ALLOCA) 的映射)
    std::unordered_map<std::string, IROperand> symbol_table;

    IRFunction(std::string name, IRType ret_type) : name(std::move(name)), return_type(ret_type) {}
};

// --- 全局变量 ---
struct IRGlobalVar {
    std::string name;
    IRType type;
    std::string initializer_str; // 字符串字面量

    IRGlobalVar(std::string n, IRType t) : name(std::move(n)), type(t) {}
    IRGlobalVar(std::string n, IRType t, std::string init)
        : name(std::move(n)), type(t), initializer_str(std::move(init)) {}
};

// --- 模块 ---
// 模块包含单个翻译单元中的所有函数和全局变量。
struct IRModule {
    std::vector<IRFunction> functions;
    std::vector<IRGlobalVar> global_vars;                    // 用于全局变量
    std::unordered_map<std::string, IROperand> symbol_table; // 全局符号 (变量/函数)

    void dump(std::ostream &os);
};

// --- 用于 IR 生成的 AST 访问者 ---
class ASTVisitor {
  public:
    virtual ~ASTVisitor() = default;

    // 主入口点
    virtual void visit(ProgramNode *node) = 0;

    // 定义
    virtual void visit(FunctionDefinitionNode *node) = 0;
    virtual void visit(VariableDeclarationListNode *node) = 0;

    // 语句
    virtual void visit(IfStatementNode *node) = 0;
    virtual void visit(WhileStatementNode *node) = 0;
    virtual void visit(ReturnStatementNode *node) = 0;
    virtual void visit(InputStatementNode *node) = 0;
    virtual void visit(OutputStatementNode *node) = 0;
    virtual void visit(BreakStatementNode *node) = 0;
    virtual void visit(ContinueStatementNode *node) = 0;

    // 表达式 (返回包含结果的操作数)
    virtual IROperand visit(AssignmentNode *node) = 0;
    virtual IROperand visit(BinaryOpNode *node) = 0;
    virtual IROperand visit(IntegerLiteralNode *node) = 0;
    virtual IROperand visit(CharacterLiteralNode *node) = 0;
    virtual IROperand visit(StringLiteralNode *node) = 0;
    virtual IROperand visit(VariableReferenceNode *node) = 0;
    virtual IROperand visit(FunctionCallNode *node) = 0;

    // 属于其他节点但本身不直接生成代码的节点
    virtual void visit(TypeSpecifierNode *) {}
    virtual void visit(ParameterDeclarationNode *) {}
    virtual void visit(VariableDefinitionNode *) {}

    // -----------------------------------------------------------------
    // --- 访问者分发逻辑 (修正版) ---
    // -----------------------------------------------------------------

    // 分发语句
    void dispatch(ASTNode *node) {
        if (!node) return;
        if (auto n = dynamic_cast<ProgramNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<FunctionDefinitionNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<VariableDeclarationListNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<IfStatementNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<WhileStatementNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<ReturnStatementNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<InputStatementNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<OutputStatementNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<BreakStatementNode *>(node)) {
            visit(n);
        } else if (auto n = dynamic_cast<ContinueStatementNode *>(node)) {
            visit(n);
        }
        // 表达式作为语句 (e.g., "a = b + 1;" 或 "foo();")
        else if (auto n = dynamic_cast<ExpressionNode *>(node)) {
            dispatch_expr(n);
        } else { /* 其他节点 (如 TypeSpecifier) 不需要显式访问 */
        }
    }

    // 分发表达式
    IROperand dispatch_expr(ASTNode *node) {
        if (!node) return {};
        if (auto n = dynamic_cast<AssignmentNode *>(node)) {
            return visit(n);
        }
        if (auto n = dynamic_cast<BinaryOpNode *>(node)) {
            return visit(n);
        }
        if (auto n = dynamic_cast<IntegerLiteralNode *>(node)) {
            return visit(n);
        }
        if (auto n = dynamic_cast<CharacterLiteralNode *>(node)) {
            return visit(n);
        }
        if (auto n = dynamic_cast<StringLiteralNode *>(node)) {
            return visit(n);
        }
        if (auto n = dynamic_cast<VariableReferenceNode *>(node)) {
            return visit(n);
        }
        if (auto n = dynamic_cast<FunctionCallNode *>(node)) {
            return visit(n);
        }
        throw std::runtime_error("Unknown expression node type in dispatch_expr");
    }
};

// --- IR 生成器 ---
class IRGenerator : public ASTVisitor {
  public:
    IRModule module;

    IRGenerator(std::unique_ptr<ASTNode> &root) {
        dispatch(root.get());
    }

    void dump_ir() {
        module.dump(std::cout);
    }

    // Visitor 实现
    void visit(ProgramNode *node) override;
    void visit(FunctionDefinitionNode *node) override;
    void visit(VariableDeclarationListNode *node) override;
    void visit(IfStatementNode *node) override;
    void visit(WhileStatementNode *node) override;
    void visit(ReturnStatementNode *node) override;
    void visit(InputStatementNode *node) override;
    void visit(OutputStatementNode *node) override;
    void visit(BreakStatementNode *node) override;
    void visit(ContinueStatementNode *node) override;

    IROperand visit(AssignmentNode *node) override;
    IROperand visit(BinaryOpNode *node) override;
    IROperand visit(IntegerLiteralNode *node) override;
    IROperand visit(CharacterLiteralNode *node) override;
    IROperand visit(StringLiteralNode *node) override;
    IROperand visit(VariableReferenceNode *node) override;
    IROperand visit(FunctionCallNode *node) override;

  private:
    IRFunction *current_function = nullptr;
    IRBasicBlock *current_block = nullptr;

    // 字符串字面量表 (map: 字符串内容 -> 全局标签名)
    std::unordered_map<std::string, std::string> string_literal_map;
    int str_literal_count = 0; // 用于命名字符串

    int reg_count = 0;   // 用于命名寄存器 %0, %1, ...
    int label_count = 0; // 用于命名标签 %L0, %L1, ...

    // 用于 break/continue 的标签栈
    std::vector<std::string> loop_start_labels;
    std::vector<std::string> loop_end_labels;

    // --- 辅助方法 ---
    IRType convert_ast_type(TypeKind type);
    IROperand new_reg(IRType type);             // 为一个值创建新寄存器
    IROperand new_reg_ptr(IRType pointee_type); // 为一个指针创建新寄存器 (来自 alloca)
    std::string new_label_name();

    void start_function(std::string name, IRType ret_type);
    void end_function();
    IRBasicBlock *create_block(std::string name = "");
    void set_current_block(IRBasicBlock *block);

    void emit(IRInstruction instruction);

    // get_var 返回变量的 *地址(指针)*
    IROperand get_var(const std::string &name);

    // [新增] 用于处理 if/while 条件
    void
    visit_condition(ASTNode *cond, const std::string &true_label, const std::string &false_label);
};
