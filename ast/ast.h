#ifndef AST_H
#define AST_H

#include <stddef.h>

// Типы узлов AST
typedef enum {
    NODE_PROGRAM,
    NODE_VARIABLE_DECLARATION,
    NODE_BINARY_OPERATION,
    NODE_LITERAL,
    NODE_IDENTIFIER,
    NODE_ASSIGNMENT,
    NODE_IF_STATEMENT,
    NODE_WHILE_LOOP,
    NODE_ROUND_LOOP,
    NODE_BLOCK,
    NODE_PRINT
} NodeType;

// Предварительное объявление структуры для рекурсивного использования
typedef struct ASTNode ASTNode;

typedef struct {
    ASTNode **items;
    size_t size;
    size_t capacity;
} NodeList;

// Структура для узла AST
struct ASTNode {
    NodeType type;
    union {
        // Для NODE_VARIABLE_DECLARATION
        struct {
            char *name;
            char *var_type;
            int is_global; // 1 = глобальная (evere), 0 = локальная (lim)
            ASTNode *initializer; // Инициализатор переменной, может быть NULL
        } variable;

        // Для NODE_BINARY_OPERATION
        struct {
            char *op_type;  // Изменил имя с operator на op_type
            ASTNode *left;
            ASTNode *right;
        } binary_op;

        // Для NODE_LITERAL
        struct {
            union {
                int int_value;
                float float_value;
                char *string_value;
            };
            char *type; // "int", "float", "string"
        } literal;

        // Для NODE_IDENTIFIER
        struct {
            char *name;
        } identifier;

        // Для NODE_ASSIGNMENT
        struct {
            char *target;
            ASTNode *value;
        } assignment;

        // Для NODE_IF_STATEMENT
        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch; // может быть NULL
        } if_stmt;

        // Для NODE_WHILE_LOOP
        struct {
            ASTNode *condition;
            ASTNode *body;
        } while_loop;

        // Для NODE_ROUND_LOOP
        struct {
            char *variable;
            ASTNode *start;
            ASTNode *end;
            ASTNode *step;
            ASTNode *body;
        } round_loop;

        // Для NODE_BLOCK и NODE_PROGRAM
        struct {
            NodeList children;
        } block;

        // Для NODE_PRINT
        struct {
            ASTNode *expression;
        } print;
    };
};

// Функции для создания узлов
ASTNode *create_program_node();

ASTNode *create_variable_declaration(const char *name, const char *var_type, int is_global);

ASTNode *
create_variable_declaration_with_init(const char *name, const char *var_type, int is_global, ASTNode *initializer);

ASTNode *create_binary_operation(const char *op_type, ASTNode *left, ASTNode *right);

ASTNode *create_literal_int(int value);

ASTNode *create_literal_float(float value);

ASTNode *create_literal_string(const char *value);

ASTNode *create_identifier_node(const char *name);

ASTNode *create_assignment_node(const char *target, ASTNode *value);

ASTNode *create_if_node(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch);

ASTNode *create_while_node(ASTNode *condition, ASTNode *body);

ASTNode *create_round_node(const char *variable, ASTNode *start, ASTNode *end, ASTNode *step, ASTNode *body);

ASTNode *create_block_node();

ASTNode *create_print_node(ASTNode *expression);

// Функции для работы со списком узлов
void add_child(ASTNode *parent, ASTNode *child);

void free_node(ASTNode *node);

// Глобальная переменная для хранения корня AST
extern ASTNode *ast_root;

#endif /* AST_H */ 