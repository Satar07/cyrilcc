%{
#include <stdlib.h>
#include "parser.h"
#define DBG 0
#define DBG_PRINT(...) do { if (DBG) fprintf(stderr, __VA_ARGS__); } while (0)
#ifdef __cplusplus
extern "C" {
#endif
int yywrap() { return 1; }
#ifdef __cplusplus
}
#endif
%}

%option yylineno
%option nounput
%option noinput

%%

"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }
"return" { return RETURN; }
"continue" { return CONTINUE; }
"break" { return BREAK; }
"int" { return INT;}
"char" { return CHAR;}
"input" { return INPUT;}
"output" { return OUTPUT;}

"+"|"-"|"*"|"/"|"("|")"|"<"|">"|"{"|"}"|";"|"="|","|":"|"["|"]"|"."|"&"|"*" {
    DBG_PRINT("SYMBOL: %s\n", yytext);
	return *yytext;
}


"==" { return EQ; }
"!=" { return NE; }
"<=" { return LE; }
">=" { return GE; }

[0-9]+ {
    DBG_PRINT("INTEGER: %s\n", yytext);
	yylval.num = atoi(yytext);
	return INTEGER;
}

\'[^\']\' {
    DBG_PRINT("CHARACTER: %s\n", yytext);
    yylval.num = yytext[1];
    return CHARACTER;
}

\"(\\.|[^\\"])*\" {
    DBG_PRINT("STRING_LITERAL: %s\n", yytext);
    yylval.str = strdup(yytext + 1);
    yylval.str[yyleng - 2] = '\0';
    return STRING_LITERAL; // TODO 需要编写转义程序
}

[ \t\r\n]


[A-Za-z]([A-Za-z]|[0-9])*	{
    DBG_PRINT("IDENTIFIER: %s\n", yytext);
    yylval.str = strdup(yytext);
    return IDENTIFIER;
}



. {
	printf("lexical error: line %d with symbol %s", yylineno, yytext);
	exit(0);
}

%%
