%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBG 1
#define DBG_PRINT(...) do { if (DBG) fprintf(stderr, __VA_ARGS__); } while (0)

extern FILE * yyin;
extern int yylineno;
extern char* yytext;

int yylex();

int yywrap() { return 1; }

void yyerror(const char* msg) {
    fprintf(stderr, "%s at '%s' (line %d)\n", msg, yytext, yylineno);
    exit(1);
}
%}
%code requires {
    #include "symbol.hpp"
    #include "ast.hpp"
}
%code {
     ASTNode_LIST* program_root = nullptr;
}

%union {
    int num;
    char* str;
    ASTNode* node;
    ASTNode_LIST* node_list;
}

%left '+' '-'
%left '*' '/'
%left LE GE EQ NE
%token <num> INTEGER
%token <str> IDENTIFIER
%token LET IF ELSE WHILE

%type <node_list> block_item_list
%type <node> block_item
%type <node> declaration
%type <node> execution
%type <node> statement
%type <node> var-declaration
%type <node> expression
%type <node> assignment comparison calculation immediate declared-var
%type <node> if-statement while-statement

%%


start: block_item_list{
    DBG_PRINT("Parsing completed.\n");
    program_root = $1;
    execute_program(program_root);

};
block_item_list: block_item{
    $$ = create_block_item_list();
    $$ = append_block_item($$, $1);
}
| block_item_list block_item{
    $$ = append_block_item($$, $2);
}
;

block_item: declaration { $$ = $1; }
| statement { $$ = $1;}
;

declaration: var-declaration { $$ = $1; }
;

statement: execution { $$ = $1; }
| if-statement { $$ = $1; }
| while-statement { $$ = $1; }
;

// 所有执行语句都是表达式
execution: expression ';'{
    $$ = create_expr_stmt_node($1);
}
;

var-declaration: LET IDENTIFIER ';' {
    DBG_PRINT("Declare %s\n", $2); 
    $$ = create_var_decl_node($2);
};

expression: assignment { $$ = $1; }
| comparison { $$ = $1; }
| calculation { $$ = $1; }
| immediate { $$ = $1; }
| declared-var { $$ = $1; }
;

assignment: IDENTIFIER '=' expression   {  
    DBG_PRINT("Assign %s \n", $1);  
    $$ = create_assignment_node($1, $3);
};

comparison: expression '<' expression   {  $$ = create_binary_op_node(OP_LT, $1, $3); }
| expression '>' expression   {  $$ = create_binary_op_node(OP_GT, $1, $3);  }
| expression LE expression   {  $$ = create_binary_op_node(OP_LE, $1, $3);  }
| expression GE expression   {  $$ = create_binary_op_node(OP_GE, $1, $3);  }
| expression EQ expression   {  $$ = create_binary_op_node(OP_EQ, $1, $3);  }
| expression NE expression   {  $$ = create_binary_op_node(OP_NE, $1, $3);  }
;


calculation: expression '+' expression   {  $$ = create_binary_op_node(OP_ADD, $1, $3);  }
| expression '-' expression   {  $$ = create_binary_op_node(OP_SUB, $1, $3);  }
| expression '*' expression   {  $$ = create_binary_op_node(OP_MUL, $1, $3);  }
| expression '/' expression   {  $$ = create_binary_op_node(OP_DIV, $1, $3);  }
;

immediate: INTEGER   {  $$ = create_integer_node($1);  };

declared-var: IDENTIFIER   { 
    $$ = create_identifier_node($1); 
}
;

if-statement: IF expression '{' block_item_list '}' {
    // DBG_PRINT("IF statement: condition = %d\n", $2);
    $$ = create_if_node($2, $4, nullptr);
}
| IF expression '{' block_item_list '}' ELSE '{' block_item_list '}' {
    // DBG_PRINT("IF-ELSE statement: condition = %d\n", $2);
    $$ = create_if_node($2, $4, $8);
}
;

while-statement: WHILE expression '{' block_item_list '}' {
    // DBG_PRINT("WHILE statement: condition = %d\n", $2);
    $$ = create_while_node($2, $4);
}
;

%%