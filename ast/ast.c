#include "ast.h"
#include <stdlib.h>
#include <string.h>

ASTNode *ast_root = NULL;

void init_node_list(NodeList *list) {
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}

void add_to_list(NodeList *list, ASTNode *node) {
    if (list->size >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        ASTNode **new_items = (ASTNode **) realloc(list->items, new_capacity * sizeof(ASTNode *));
        if (new_items) {
            list->items = new_items;
            list->capacity = new_capacity;
        } else {
            return;
        }
    }
    list->items[list->size++] = node;
}

char *strdup_custom(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *new_str = (char *) malloc(len + 1);
    if (new_str) {
        strcpy(new_str, str);
    }
    return new_str;
}

ASTNode *create_program_node() {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_PROGRAM;
        init_node_list(&node->block.children);
    }
    return node;
}

ASTNode *create_variable_declaration(const char *name, const char *var_type, int is_global) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_VARIABLE_DECLARATION;
        node->variable.name = strdup_custom(name);
        node->variable.var_type = strdup_custom(var_type);
        node->variable.is_global = is_global;
        node->variable.initializer = NULL;
    }
    return node;
}

ASTNode *
create_variable_declaration_with_init(const char *name, const char *var_type, int is_global, ASTNode *initializer) {
    ASTNode *node = create_variable_declaration(name, var_type, is_global);
    if (node) {
        node->variable.initializer = initializer;
    }
    return node;
}

ASTNode *create_binary_operation(const char *op_type, ASTNode *left, ASTNode *right) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_BINARY_OPERATION;
        node->binary_op.op_type = strdup_custom(op_type);
        node->binary_op.left = left;
        node->binary_op.right = right;
    }
    return node;
}

ASTNode *create_literal_int(int value) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_LITERAL;
        node->literal.int_value = value;
        node->literal.type = strdup_custom("int");
    }
    return node;
}

ASTNode *create_literal_float(float value) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_LITERAL;
        node->literal.float_value = value;
        node->literal.type = strdup_custom("float");
    }
    return node;
}

ASTNode *create_literal_string(const char *value) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_LITERAL;
        node->literal.string_value = strdup_custom(value);
        node->literal.type = strdup_custom("string");
    }
    return node;
}

ASTNode *create_identifier_node(const char *name) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_IDENTIFIER;
        node->identifier.name = strdup_custom(name);
    }
    return node;
}

ASTNode *create_assignment_node(const char *target, ASTNode *value) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_ASSIGNMENT;
        node->assignment.target = strdup_custom(target);
        node->assignment.value = value;
    }
    return node;
}

ASTNode *create_if_node(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_IF_STATEMENT;
        node->if_stmt.condition = condition;
        node->if_stmt.then_branch = then_branch;
        node->if_stmt.else_branch = else_branch;
    }
    return node;
}

ASTNode *create_while_node(ASTNode *condition, ASTNode *body) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_WHILE_LOOP;
        node->while_loop.condition = condition;
        node->while_loop.body = body;
    }
    return node;
}

ASTNode *create_round_node(const char *variable, ASTNode *start, ASTNode *end, ASTNode *step, ASTNode *body) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_ROUND_LOOP;
        node->round_loop.variable = strdup_custom(variable);
        node->round_loop.start = start;
        node->round_loop.end = end;
        node->round_loop.step = step;
        node->round_loop.body = body;
    }
    return node;
}

ASTNode *create_block_node() {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_BLOCK;
        init_node_list(&node->block.children);
    }
    return node;
}

ASTNode *create_print_node(ASTNode *expression) {
    ASTNode *node = (ASTNode *) malloc(sizeof(ASTNode));
    if (node) {
        node->type = NODE_PRINT;
        node->print.expression = expression;
    }
    return node;
}

void add_child(ASTNode *parent, ASTNode *child) {
    if (parent && child && (parent->type == NODE_BLOCK || parent->type == NODE_PROGRAM)) {
        add_to_list(&parent->block.children, child);
    }
}

void free_node(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK:
            for (size_t i = 0; i < node->block.children.size; i++) {
                free_node(node->block.children.items[i]);
            }
            free(node->block.children.items);
            break;

        case NODE_VARIABLE_DECLARATION:
            free(node->variable.name);
            free(node->variable.var_type);
            if (node->variable.initializer) {
                free_node(node->variable.initializer);
            }
            break;

        case NODE_BINARY_OPERATION:
            free(node->binary_op.op_type);
            free_node(node->binary_op.left);
            free_node(node->binary_op.right);
            break;

        case NODE_LITERAL:
            if (strcmp(node->literal.type, "string") == 0) {
                free(node->literal.string_value);
            }
            free(node->literal.type);
            break;

        case NODE_IDENTIFIER:
            free(node->identifier.name);
            break;

        case NODE_ASSIGNMENT:
            free(node->assignment.target);
            free_node(node->assignment.value);
            break;

        case NODE_IF_STATEMENT:
            free_node(node->if_stmt.condition);
            free_node(node->if_stmt.then_branch);
            if (node->if_stmt.else_branch) {
                free_node(node->if_stmt.else_branch);
            }
            break;

        case NODE_WHILE_LOOP:
            free_node(node->while_loop.condition);
            free_node(node->while_loop.body);
            break;

        case NODE_ROUND_LOOP:
            free(node->round_loop.variable);
            free_node(node->round_loop.start);
            free_node(node->round_loop.end);
            free_node(node->round_loop.step);
            free_node(node->round_loop.body);
            break;

        case NODE_PRINT:
            free_node(node->print.expression);
            break;
    }

    free(node);
} 