%{
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../ast.h"

// Объявления функций
extern int yylex();
extern int yyparse();
extern void yyerror(const char* s);
extern FILE* yyin;

// Глобальный корень AST определен в ast.c
extern ASTNode* ast_root;
%}


%union{
   int ival;
   float fval;
   char* sval;
   ASTNode* node;
}

%token <ival> INT_LITERAL
%token <fval> FLOAT_LITERAL
%token <sval> IDENTIFIER STRING_LITERAL COMPARE

%token IF ELSE WHILE ROUND IN RANGE
%token EVERE LIM PRINT
%token AND OR
%token INT FLOAT STRING
%token UNKNOWN

%type <node> program operation_list operation declaration
%type <node> expr print_opr if_opr while_opr round_opr block_stmt
%type <node> assignment scope_type

%right '='
%left '+' '-'
%left '*' '/'
%left '.'

%start program

%%

program
    : operation_list  { ast_root = $1; }
    ;

operation_list
    : operation_list operation 
    { 
        $$ = $1; 
        if ($2) { add_child($$, $2); }
    }
    | operation 
    { 
        $$ = create_program_node(); 
        if ($1) { add_child($$, $1); }
    }
    ;

operation
    : declaration ';'    { $$ = $1; }
    | assignment ';'     { $$ = $1; }
    | print_opr ';'      { $$ = $1; }
    | if_opr             { $$ = $1; }
    | while_opr          { $$ = $1; }
    | round_opr          { $$ = $1; }
    | block_stmt         { $$ = $1; }
    ;

scope_type
    : EVERE { $$ = create_literal_int(1); } /* evere = глобальная */
    | LIM   { $$ = create_literal_int(0); } /* lim = локальная */
    ;

declaration
    : INT scope_type IDENTIFIER  
    { 
        int is_global = $2->literal.int_value;
        $$ = create_variable_declaration($3, "int", is_global);
        free_node($2);
        free($3);
    }
    | FLOAT scope_type IDENTIFIER  
    { 
        int is_global = $2->literal.int_value;
        $$ = create_variable_declaration($3, "float", is_global);
        free_node($2);
        free($3);
    }
    | STRING scope_type IDENTIFIER  
    { 
        int is_global = $2->literal.int_value;
        $$ = create_variable_declaration($3, "string", is_global);
        free_node($2);
        free($3);
    }
    | INT scope_type IDENTIFIER '=' expr
    { 
        int is_global = $2->literal.int_value;
        $$ = create_variable_declaration_with_init($3, "int", is_global, $5);
        free_node($2);
        free($3);
    }
    | FLOAT scope_type IDENTIFIER '=' expr
    { 
        int is_global = $2->literal.int_value;
        $$ = create_variable_declaration_with_init($3, "float", is_global, $5);
        free_node($2);
        free($3);
    }
    | STRING scope_type IDENTIFIER '=' expr
    { 
        int is_global = $2->literal.int_value;
        $$ = create_variable_declaration_with_init($3, "string", is_global, $5);
        free_node($2);
        free($3);
    }
    ;

assignment
    : IDENTIFIER '=' expr 
    { 
        $$ = create_assignment_node($1, $3);
        free($1);
    }
    ;

print_opr
    : PRINT '(' expr ')'
    { 
        $$ = create_print_node($3);
    }
    ;

if_opr
    : IF '(' expr ')' operation
    { 
        $$ = create_if_node($3, $5, NULL);
    }
    | IF '(' expr ')' operation ELSE operation
    { 
        $$ = create_if_node($3, $5, $7);
    }
    ;

while_opr
    : WHILE '(' expr ')' operation
    { 
        $$ = create_while_node($3, $5);
    }
    ;

round_opr
    : ROUND IDENTIFIER IN RANGE '(' expr ',' expr ',' expr ')' operation
    { 
        $$ = create_round_node($2, $6, $8, $10, $12);
        free($2);
    }
    ;

block_stmt
    : '{' operation_list '}'
    { 
        $$ = create_block_node();
        // Копируем дочерние узлы из operation_list в block
        if ($2 && ($2->type == NODE_PROGRAM || $2->type == NODE_BLOCK)) {
            NodeList* children = &$2->block.children;
            for (size_t i = 0; i < children->size; i++) {
                add_child($$, children->items[i]);
            }
            // Очищаем родительский узел, но не его детей (они теперь принадлежат новому блоку)
            children->size = 0;
            free_node($2);
        }
    }
    ;

expr
    : expr '+' expr
    { 
        $$ = create_binary_operation("+", $1, $3);
    }
    | expr '-' expr
    { 
        $$ = create_binary_operation("-", $1, $3);
    }
    | expr '*' expr
    { 
        $$ = create_binary_operation("*", $1, $3);
    }
    | expr '/' expr
    { 
        $$ = create_binary_operation("/", $1, $3);
    }
    | expr '.' expr
    { 
        $$ = create_binary_operation(".", $1, $3);
    }
    | expr COMPARE expr
    { 
        $$ = create_binary_operation($2, $1, $3);
        free($2);
    }
    | expr AND expr
    { 
        $$ = create_binary_operation("and", $1, $3);
    }
    | expr OR expr
    { 
        $$ = create_binary_operation("or", $1, $3);
    }
    | '(' expr ')'
    { 
        $$ = $2;
    }
    | INT_LITERAL
    { 
        $$ = create_literal_int($1);
    }
    | FLOAT_LITERAL
    { 
        $$ = create_literal_float($1);
    }
    | STRING_LITERAL
    { 
        $$ = create_literal_string($1);
        free($1);
    }
    | IDENTIFIER
    { 
        $$ = create_identifier_node($1);
        free($1);
    }
    ;

%%

void yyerror(const char* s) {
    fprintf(stderr, "Parser error: %s\n", s);
    exit(1);
}

// Инициализация парсера
int parser_init(const char* filename) {
    // Открытие входного файла
    FILE* input_file = fopen(filename, "r");
    if (!input_file) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return 1;
    }
    yyin = input_file;
    
    // Старт парсера
    yyparse();
    
    // Закрытие файла
    fclose(input_file);
    
    return 0;
}

// Получение корня AST
ASTNode* get_ast_root() {
    return ast_root;
}
