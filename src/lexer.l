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

%%

"let" { return LET; }
"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }

"+"|"-"|"*"|"/"|"("|")"|"<"|">"|"{"|"}"|";"|"=" {
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