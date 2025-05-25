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

typedef struct ASTNode ASTNode;

typedef struct {
    ASTNode **items;
    size_t size;
    size_t capacity;
} NodeList;

struct ASTNode {
    NodeType type;
    union {
        struct {
            char *name;
            char *var_type;
            int is_global;
            ASTNode *initializer;
        } variable;

        struct {
            char *op_type;
            ASTNode *left;
            ASTNode *right;
        } binary_op;

        struct {
            union {
                int int_value;
                float float_value;
                char *string_value;
            };
            char *type;
        } literal;

        struct {
            char *name;
        } identifier;

        struct {
            char *target;
            ASTNode *value;
        } assignment;

        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_stmt;

        struct {
            ASTNode *condition;
            ASTNode *body;
        } while_loop;

        struct {
            char *variable;
            ASTNode *start;
            ASTNode *end;
            ASTNode *step;
            ASTNode *body;
        } round_loop;

        struct {
            NodeList children;
        } block;

        struct {
            ASTNode *expression;
        } print;
    };
};

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

void add_child(ASTNode *parent, ASTNode *child);

void free_node(ASTNode *node);

extern ASTNode *ast_root;

#endif /* AST_H */ 