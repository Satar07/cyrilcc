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
    #include "ast.hpp"
}
%code {
}

%expect 16

%union {
    int num;
    char* str;
    ASTNode* node;
    ASTNode_List* node_list;
}

/* 运算符优先级 */
%right '='
%left '<' '>' LE GE EQ NE
%left '+' '-'
%left '*' '/'


/* 基础类型 */
%token <str> IDENTIFIER
%token <num> INTEGER
%token <num> CHARACTER
%token <str> STRING_LITERAL

/* 关键字 类型 */
%token INT CHAR

/* 关键字 控制流 */
%token IF ELSE WHILE FOR SWITCH CASE DEFAULT
%token INPUT OUTPUT
%token CONTINUE BREAK RETURN

/* 顶层结构 */
%type <node_list> start definition_list
%type <node> definition

/* 类型定义 默认int */
%type <node> type_specifier optional_type_specifier

/* 函数定义 */
%type <node> function_definition
%type <node> function_parameter_declaration
%type <node_list> function_parameter_declaration_list optional_function_parameter_declaration_list

/* 变量定义 */
%type <node_list> var_definition_list_inner
%type <node> var_definition_single
%type <node> var_definition

/* 语句块 */
%type <node_list> block_item_list
%type <node> block_item

/* 语句 */
%type <node> statement
%type <node> input_statement output_statement
%type <node> if_statement while_statement for_statement
%type <node> switch_statement case_statement
%type <node_list> case_statement_list
%type <node> return_statement break_statement continue_statement

/* 表达式 */
%type <node> expression optional_expression
%type <node> assignment comparison calculation immediate

/* 变量调用 */
%type <node> declared_var

/* 函数调用 */
%type <node> function_call
%type <node_list> function_call_arg_list optional_function_call_arg_list

%%

/* 顶层结构 */
start: definition_list {
    DBG_PRINT("Parsing completed.");
    ast_setup_program($1);
}
;

definition_list: definition {
    $$ = ast_list_create($1);
}
| definition_list definition {
    $$ = ast_list_append($1, $2);
}
;

definition: var_definition ';' { $$ = $1; }
| function_definition { $$ = $1; }
;

/* 类型定义 */
type_specifier: INT {
    $$ = ast_create_type_int();
}
| CHAR {
    $$ = ast_create_type_char();
}
;

optional_type_specifier : {
    $$ = ast_create_type_int();
}
| type_specifier {
    $$ = $1;
}
;

/* 函数定义 这里默认返回int TODO 可能会*/
function_definition: IDENTIFIER '(' optional_function_parameter_declaration_list ')' '{' block_item_list '}' {
    $$ = ast_create_definition_function(ast_create_type_int(), $1, $3, $6);
}
;

optional_function_parameter_declaration_list: {
    $$ = ast_list_create_empty();
}
| function_parameter_declaration_list {
    $$ = $1;
}
;

function_parameter_declaration_list: function_parameter_declaration {
    $$ = ast_list_create($1);
}
| function_parameter_declaration_list ',' function_parameter_declaration {
    $$ = ast_list_append($1, $3);
}
;

function_parameter_declaration: optional_type_specifier IDENTIFIER {
    $$ = ast_create_declaration_parameter($1, $2);
}
;

/* 变量定义 */
var_definition: type_specifier var_definition_list_inner {
    $$ = ast_create_definition_variable_list($1, $2);
}
;

var_definition_list_inner: var_definition_single {
    $$ = ast_list_create($1);
}
| var_definition_list_inner ',' var_definition_single {
    $$ = ast_list_append($1, $3);
}
;

var_definition_single: IDENTIFIER {
    $$ = ast_create_definition_variable($1, nullptr);
}
| IDENTIFIER '=' expression {
    $$ = ast_create_definition_variable($1, $3);
}
;


/* 语句块 */
block_item_list: block_item {
    $$ = ast_list_create($1);
}
| block_item_list block_item {
    $$ = ast_list_append($1, $2);
}
;

block_item: var_definition ';' {
    $$ = $1;
}
| statement {
    $$ = $1;
}
;

/* 语句 */
statement: expression ';' {
    $$ = $1;
}
| input_statement ';' {
    $$ = $1;
}
| output_statement ';' {
    $$ = $1;
}
| if_statement {
    $$ = $1;
}
| while_statement {
    $$ = $1;
}
| for_statement {
    $$ = $1;
}
| switch_statement {
    $$ = $1;
}
| return_statement ';' {
    $$ = $1;
}
| break_statement ';' {
    $$ = $1;
}
| continue_statement ';' {
    $$ = $1;
}
;

input_statement: INPUT expression {
    $$ = ast_create_statement_input($2);
}
;

output_statement: OUTPUT expression {
    $$ = ast_create_statement_output($2);
}
;

if_statement: IF '(' expression ')' '{' block_item_list '}' {
    $$ = ast_create_statement_if($3, $6);
}
| IF '(' expression ')' '{' block_item_list '}' ELSE '{' block_item_list '}' {
    $$ = ast_create_statement_if_else($3, $6, $10);
}
;

while_statement: WHILE '(' expression ')' '{' block_item_list '}' {
    $$ = ast_create_statement_while($3, $6);
}
;

for_statement: FOR '(' var_definition ';' optional_expression ';' optional_expression ')' '{' block_item_list '}' {
    $$ = ast_create_statement_for($3, $5, $7, $10);
}
| FOR '(' optional_expression ';' optional_expression ';' optional_expression ')' '{' block_item_list '}' {
    $$ = ast_create_statement_for($3, $5, $7, $10);
}
;

return_statement: RETURN expression {
    $$ = ast_create_statement_return($2);
}
;

switch_statement : SWITCH '(' expression ')' '{' case_statement_list '}' {
    $$ = ast_create_statement_switch($3, $6);
}
;

case_statement_list : {
    $$ = ast_list_create_empty();
}
| case_statement_list case_statement {
    $$ = ast_list_append($1, $2);
}
;

case_statement : CASE INTEGER ':'  {
    $$ = ast_create_statement_case($2);
}
| CASE CHARACTER ':' {
    $$ = ast_create_statement_case($2);
}
| DEFAULT ':' {
    $$ = ast_create_statement_default();
}
| block_item_list {
    // 这里会产生SR警告，是正常的，因为没有大括号，不知道是否需要移入还是结束这个list，默认移入是对的
    $$ = ast_create_statement_case_block($1);
}
| '{' block_item_list '}' {
    $$ = ast_create_statement_case_block($2);
}
;

break_statement: BREAK {
    $$ = ast_create_statement_break();
}
;

continue_statement: CONTINUE {
    $$ = ast_create_statement_continue();
}
;

/* 表达式 */
optional_expression: { $$ = nullptr; }
| expression { $$ = $1; }
;


expression: assignment { $$ = $1; }
| comparison { $$ = $1; }
| calculation { $$ = $1; }
;

assignment: declared_var '=' expression {
    $$ = ast_create_assignment($1, $3);
}
;

comparison: calculation '<' calculation {
    $$ = ast_create_comparison_lt($1, $3);
}
| calculation '>' calculation {
    $$ = ast_create_comparison_gt($1, $3);
}
| calculation LE calculation {
    $$ = ast_create_comparison_le($1, $3);
}
| calculation GE calculation {
    $$ = ast_create_comparison_ge($1, $3);
}
| calculation EQ calculation {
    $$ = ast_create_comparison_eq($1, $3);
}
| calculation NE calculation {
    $$ = ast_create_comparison_ne($1, $3);
}
;

calculation: calculation '+' calculation {
    $$ = ast_create_calculation_add($1, $3);
}
| calculation '-' calculation {
    $$ = ast_create_calculation_sub($1, $3);
}
| calculation '*' calculation {
    $$ = ast_create_calculation_mul($1, $3);
}
| calculation '/' calculation {
    $$ = ast_create_calculation_div($1, $3);
}
| immediate { $$ = $1; }
| declared_var { $$ = $1; }
| function_call { $$ = $1; }
| '(' expression ')' { $$ = $2; }
;

immediate: INTEGER {
    $$ = ast_create_immediate_integer($1);
}
| CHARACTER {
    $$ = ast_create_immediate_character($1);
}
| STRING_LITERAL {
    $$ = ast_create_immediate_string($1);
}
;

/* 变量调用 */
declared_var: IDENTIFIER {
    $$ = ast_create_variable_reference($1);
}
;

/* 函数调用 */
function_call: IDENTIFIER '(' optional_function_call_arg_list ')' {
    $$ = ast_create_function_call($1, $3);
}
;

optional_function_call_arg_list: {
    $$ = ast_list_create_empty();
}
| function_call_arg_list {
    $$ = $1;
}
;

function_call_arg_list: expression {
    $$ = ast_list_create($1);
}
| function_call_arg_list ',' expression {
    $$ = ast_list_append($1, $3);
}
;

%%
