%{
#include "../ast.h"
#include "../parser/parser.tab.h" 
#include <string.h>

// Добавляем переменные для отслеживания позиции в файле
int line_num = 1;
int column_num = 1;
char current_line[1024] = "";
int line_len = 0;

// Функция для обновления столбца
void update_column() {
    column_num += yyleng;
}

// Функция для обновления строки
void update_line() {
    line_num++;
    column_num = 1;
}

// Функция для получения текущей позиции токена
void get_token_position(int *line, int *column) {
    if (line) *line = line_num;
    if (column) *column = column_num;
}

// Экспортируем текущие координаты в коде
int get_current_line() { return line_num; }
int get_current_column() { return column_num; }
%}

%x COMMENT

%%

"if"      {update_column(); return IF; }
"else"    {update_column(); return ELSE; }
"while"   {update_column(); return WHILE; }
"round"   {update_column(); return ROUND; }
"in"      {update_column(); return IN; }
"range"   {update_column(); return RANGE; }
"evere"   {update_column(); return EVERE; }
"lim"     {update_column(); return LIM; }
"int"     {update_column(); return INT; }
"string"  {update_column(); return STRING; }
"and"     {update_column(); return AND; }
"or"      {update_column(); return OR; }
"print"   {update_column(); return PRINT; }

[0-9]+                { update_column(); yylval.ival = atoi(yytext); return INT_LITERAL; }
\"[^\"]*\"            { update_column(); yylval.sval = strdup(yytext); return STRING_LITERAL; }

[a-zA-Z_][a-zA-Z0-9_]* { update_column(); yylval.sval = strdup(yytext); return IDENTIFIER; }

"=="                  { update_column(); yylval.sval = strdup(yytext); return COMPARE; }
"!="                  { update_column(); yylval.sval = strdup(yytext); return COMPARE; }
"<="                  { update_column(); yylval.sval = strdup(yytext); return COMPARE; }
">="                  { update_column(); yylval.sval = strdup(yytext); return COMPARE; }
"<"                   { update_column(); yylval.sval = strdup(yytext); return COMPARE; }
">"                   { update_column(); yylval.sval = strdup(yytext); return COMPARE; }

"+"                   { update_column(); return '+'; }
"-"                   { update_column(); return '-'; }
"*"                   { update_column(); return '*'; }
"/"                   { update_column(); return '/'; }
"%"                   { update_column(); return '%'; }
"="                   { update_column(); return '='; }
"."                   { update_column(); return '.'; }

"("                   { update_column(); return '('; }
")"                   { update_column(); return ')'; }
"{"                   { update_column(); return '{'; }
"}"                   { update_column(); return '}'; }
";"                   { update_column(); return ';'; }
","                   { update_column(); return ','; }

[ \t]+                { update_column(); /* Пробелы — игнорировать*/ }
[\r]                  { column_num = 1; /* Возврат каретки */ }
[\n]                  { update_line(); /* Перенос строки */ }

.                     { update_column(); return UNKNOWN;}

"//".*                { update_column(); /* игнорируем однострочный комментарий */ }
"/*"                  { update_column(); BEGIN(COMMENT); }
<COMMENT>"*/"         { update_column(); BEGIN(INITIAL); }
<COMMENT>.            { update_column(); /* внутри комментария */ }
<COMMENT>\n           { update_line(); /* внутри комментария */ }

%%

int yywrap(void) {
    return 1;
}