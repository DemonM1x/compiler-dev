%{
#include "../ast.h"
#include "../parser/parser.tab.h" 
#include <string.h>
%}

%x COMMENT

%%

"if"      {return IF; }
"else"    {return ELSE; }
"while"   {return WHILE; }
"round"   {return ROUND; }
"in"      {return IN; }
"range"   {return RANGE; }
"evere"   {return EVERE; }
"lim"     {return LIM; }
"int"     {return INT; }
"float"   {return FLOAT; }
"string"  {return STRING; }
"and"     { return AND; }
"or"      { return OR; }
"print"   { return PRINT; }

[0-9]+\.[0-9]+        { yylval.fval = atof(yytext); return FLOAT_LITERAL; }
[0-9]+                { yylval.ival = atoi(yytext); return INT_LITERAL; }
\"[^\"]*\"            { yylval.sval = strdup(yytext); return STRING_LITERAL; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval.sval = strdup(yytext); return IDENTIFIER; }

"=="                            { yylval.sval = strdup(yytext); return COMPARE; }
"!="                            { yylval.sval = strdup(yytext); return COMPARE; }
"<="                            { yylval.sval = strdup(yytext); return COMPARE; }
">="                            { yylval.sval = strdup(yytext); return COMPARE; }
"<"                             { yylval.sval = strdup(yytext); return COMPARE; }
">"                             { yylval.sval = strdup(yytext); return COMPARE; }

"+"                             { return '+'; }
"-"                             { return '-'; }
"*"                             { return '*'; }
"/"                             { return '/'; }
"="                             { return '='; }
"."                             { return '.'; }

"("       { return '('; }
")"       { return ')'; }
"{"       { return '{'; }
"}"       { return '}'; }
";"       { return ';'; }
","       { return ','; }

[ \t\r\n]+                         { /* Пробелы и переносы строк — игнорировать*/ }

.                                  { return UNKNOWN;}

"//".*             { /* игнорируем однострочный комментарий */ }
"/*"               { BEGIN(COMMENT); }
<COMMENT>"*/"      { BEGIN(INITIAL); }
<COMMENT>.|\n      { /* внутри комментария */ }

%%

int yywrap(void) {
    return 1;
}