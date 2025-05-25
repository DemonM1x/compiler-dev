#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "risc_generator.h"
#include "../ast/ast.h"
#include "../error_handler.h"
extern int get_current_line(void);
extern int get_current_column(void);
extern const char* get_parser_filename(void);

// Прототипы функций

typedef struct {
    char **output;
    size_t output_size;
    size_t output_capacity;
    struct {
        char *name;
        char *type;
        int is_global;
        int block_level;
    } *variables;
    size_t var_count;
    size_t var_capacity;
    int temp_counter;
    int label_counter;
    int data_counter;
    int block_level;
    int memory_pos;
    int temp_string_pos;
    struct {
        char *name;
        int address;
    } *var_addresses;
    size_t addr_count;
    size_t addr_capacity;
    char current_file[256];
    int current_scope_is_global;
} RISCGenerator;

static void register_variable(RISCGenerator *gen, const char *name, const char *type, int is_global);
static int register_variable_address(RISCGenerator *gen, const char *name);
static void process_variable_declaration(RISCGenerator *gen, ASTNode *node);
static void process_assignment(RISCGenerator *gen, ASTNode *node);
static void process_if_statement(RISCGenerator *gen, ASTNode *node);
static void process_while_loop(RISCGenerator *gen, ASTNode *node);
static void process_round_loop(RISCGenerator *gen, ASTNode *node);
static void process_print(RISCGenerator *gen, ASTNode *node);
static void evaluate_expression(RISCGenerator *gen, ASTNode *node, const char *target_reg);
static int add_string_literal(RISCGenerator *gen, const char *str);
static int concatenate_strings(RISCGenerator *gen, const char *reg1, const char *reg2);
static int is_string_variable(RISCGenerator *gen, const char *name);
static int declare_variable(RISCGenerator *gen, const char *name, const char *type, int is_global);
static int check_division_by_zero(RISCGenerator *gen, ASTNode *left, ASTNode *right, const char *op);

static char current_filename[256] = "unknown";

void set_risc_generator_filename(const char *filename) {
    if (filename) {
        strncpy(current_filename, filename, sizeof(current_filename) - 1);
        current_filename[sizeof(current_filename) - 1] = '\0';
    } else {
        strcpy(current_filename, "unknown");
    }
}

static RISCGenerator *init_generator(const char *filename) {
    RISCGenerator *gen = (RISCGenerator *) malloc(sizeof(RISCGenerator));
    if (!gen) return NULL;
    gen->output = NULL;
    gen->output_size = 0;
    gen->output_capacity = 0;
    gen->variables = NULL;
    gen->var_count = 0;
    gen->var_capacity = 0;
    gen->temp_counter = 0;
    gen->label_counter = 0;
    gen->data_counter = 0;
    gen->block_level = 0;
    gen->memory_pos = 1000;
    gen->temp_string_pos = 2000;
    gen->var_addresses = NULL;
    gen->addr_count = 0;
    gen->addr_capacity = 0;
    if (filename) {
        strncpy(gen->current_file, filename, sizeof(gen->current_file) - 1);
        gen->current_file[sizeof(gen->current_file) - 1] = '\0';
    } else {
        strcpy(gen->current_file, "unknown");
    }
    gen->current_scope_is_global = 1;
    error_init();
    return gen;
}

static void free_generator(RISCGenerator *gen) {
    if (gen->output) {
        for (size_t i = 0; i < gen->output_size; i++) {
            free(gen->output[i]);
        }
        free(gen->output);
    }
    if (gen->variables) {
        for (size_t i = 0; i < gen->var_count; i++) {
            free(gen->variables[i].name);
            free(gen->variables[i].type);
        }
        free(gen->variables);
    }
    if (gen->var_addresses) {
        for (size_t i = 0; i < gen->addr_count; i++) {
            free(gen->var_addresses[i].name);
        }
        free(gen->var_addresses);
    }
    free(gen);
}

static void add_output(RISCGenerator *gen, const char *line) {
    if (gen->output_size >= gen->output_capacity) {
        size_t new_capacity = gen->output_capacity == 0 ? 16 : gen->output_capacity * 2;
        char **new_output = (char **) realloc(gen->output, new_capacity * sizeof(char *));
        if (!new_output) return;
        gen->output = new_output;
        gen->output_capacity = new_capacity;
    }
    gen->output[gen->output_size] = strdup(line);
    if (!gen->output[gen->output_size]) return;
    gen->output_size++;
}

static int get_variable_address(RISCGenerator *gen, const char *name) {
    int line = get_current_line();
    int column = get_current_column();
    int found = 0;
    int is_global = 0;
    int var_block_level = -1;
    size_t var_index = 0;
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0) {
            found = 1;
            is_global = gen->variables[i].is_global;
            var_block_level = gen->variables[i].block_level;
            var_index = i;
            break;
        }
    }
    if (!found) {
        error_report(ERROR_UNDEFINED_VARIABLE, line, column, gen->current_file,
                   "Variable '%s' is not defined", name);
        return -1;
    }
    if (!is_global) {
        if (gen->current_scope_is_global) {
            error_report(ERROR_SCOPE, line, column, gen->current_file,
                       "Cannot access local variable '%s' from global scope", name);
            return -1;
        }
        if (var_block_level > gen->block_level) {
            error_report(ERROR_SCOPE, line, column, gen->current_file,
                       "Cannot access variable '%s' outside its declaring block", name);
            return -1;
        }
    }
    for (size_t i = 0; i < gen->addr_count; i++) {
        if (strcmp(gen->var_addresses[i].name, name) == 0) {
            return gen->var_addresses[i].address;
        }
    }
    return register_variable_address(gen, name);
}

static int register_variable_address(RISCGenerator *gen, const char *name) {
    if (!gen || !name) return -1;
    if (gen->addr_count >= gen->addr_capacity) {
        size_t new_capacity = gen->addr_capacity == 0 ? 8 : gen->addr_capacity * 2;
        void *new_addrs = realloc(gen->var_addresses, new_capacity * sizeof(*gen->var_addresses));
        if (!new_addrs) return -1;
        gen->var_addresses = new_addrs;
        gen->addr_capacity = new_capacity;
    }
    gen->var_addresses[gen->addr_count].name = strdup(name);
    gen->var_addresses[gen->addr_count].address = gen->memory_pos;
    gen->memory_pos += 1;
    return gen->var_addresses[gen->addr_count++].address;
}

static char *get_new_temp(RISCGenerator *gen) {
    char temp_name[32];
    snprintf(temp_name, sizeof(temp_name), "__tmp%d", gen->temp_counter++);
    register_variable(gen, temp_name, "int", 0);
    register_variable_address(gen, temp_name);
    return strdup(temp_name);
}

static char *get_new_label(RISCGenerator *gen, const char *prefix) {
    char label_name[64];
    snprintf(label_name, sizeof(label_name), "__%s_%d", prefix, gen->label_counter++);
    return strdup(label_name);
}

static void register_variable(RISCGenerator *gen, const char *name, const char *type, int is_global) {
    if (!name) return;
    if (gen->var_count >= gen->var_capacity) {
        size_t new_capacity = gen->var_capacity == 0 ? 8 : gen->var_capacity * 2;
        void *new_vars = realloc(gen->variables, new_capacity * sizeof(*gen->variables));
        if (!new_vars) return;
        gen->variables = new_vars;
        gen->var_capacity = new_capacity;
    }
    gen->variables[gen->var_count].name = strdup(name);
    gen->variables[gen->var_count].type = type ? strdup(type) : strdup("unknown");
    gen->variables[gen->var_count].is_global = is_global;
    gen->variables[gen->var_count].block_level = is_global ? 0 : gen->block_level;
    gen->var_count++;
}

static int declare_variable(RISCGenerator *gen, const char *name, const char *type, int is_global) {
    if (!gen || !name) return -1;
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0 && 
            gen->variables[i].is_global == is_global) {
            error_report(ERROR_REDECLARATION, 0, 0, gen->current_file,
                        "Variable '%s' is already declared in this scope", name);
            return -1;
        }
    }
    if (is_global && !gen->current_scope_is_global) {
        error_report(ERROR_SCOPE, 0, 0, gen->current_file,
                    "Cannot declare global variable '%s' in local scope", name);
        return -1;
    }
    register_variable(gen, name, type, is_global);
    return 0;
}

static void process_variable_declaration(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;
    int line = get_current_line();
    int column = get_current_column();
    const char *name = node->variable.name;
    const char *type = node->variable.var_type;
    int is_global = node->variable.is_global;
    if (is_global && !gen->current_scope_is_global) {
        error_report(ERROR_SCOPE, line, column, gen->current_file,
                    "Cannot declare global variable '%s' in local scope", name);
        return;
    }
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0 && 
            gen->variables[i].is_global == is_global) {
            error_report(ERROR_REDECLARATION, line, column, gen->current_file,
                        "Variable '%s' is already declared in this scope", name);
            return;
        }
    }
    register_variable(gen, name, type, is_global);
    int var_addr = register_variable_address(gen, name);
    if (node->variable.initializer) {
        char buffer[256];
        if (node->variable.initializer->type == NODE_LITERAL) {
            if (!error_check_type_compatibility(
                type,
                node->variable.initializer->literal.type,
                "=", 0, 0, gen->current_file
            )) {
                return;
            }
        } 
        else if (node->variable.initializer->type == NODE_IDENTIFIER) {
            const char *source_type = NULL;
            for (size_t i = 0; i < gen->var_count; i++) {
                if (strcmp(gen->variables[i].name, 
                        node->variable.initializer->identifier.name) == 0) {
                    source_type = gen->variables[i].type;
                    break;
                }
            }
            if (source_type) {
                if (!error_check_type_compatibility(
                    type, source_type, "=", 0, 0, gen->current_file
                )) {
                    return;
                }
            } else {
                error_report(ERROR_UNDEFINED_VARIABLE, line, column, gen->current_file, 
                          "Variable '%s' is not defined", 
                          node->variable.initializer->identifier.name);
                return;
            }
        }
        if (!error_is_critical()) {
            snprintf(buffer, sizeof(buffer), "Initialize %s (address %d)", name, var_addr);
            add_output(gen, buffer);
            evaluate_expression(gen, node->variable.initializer, "x1");
            snprintf(buffer, sizeof(buffer), "li x2, %d", var_addr);
            add_output(gen, buffer);
            add_output(gen, "sw x2, 0, x1");
        }
    } else {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Initialize %s with default value 0 (address %d)", name, var_addr);
        add_output(gen, buffer);
        add_output(gen, "li x1, 0");
        snprintf(buffer, sizeof(buffer), "li x2, %d", var_addr);
        add_output(gen, buffer);
        add_output(gen, "sw x2, 0, x1");
    }
}

static void process_node(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) {
        add_output(gen, "Warning: NULL node detected!");
        return;
    }
    char buffer[1024];
    switch (node->type) {
        case NODE_PROGRAM:
            gen->current_scope_is_global = 1;
            gen->block_level = 0;
            for (size_t i = 0; i < node->block.children.size; i++) {
                process_node(gen, node->block.children.items[i]);
            }
            break;
        case NODE_VARIABLE_DECLARATION:
            process_variable_declaration(gen, node);
            break;
        case NODE_ASSIGNMENT:
            process_assignment(gen, node);
            break;
        case NODE_BINARY_OPERATION:
            break;
        case NODE_LITERAL:
            break;
        case NODE_IDENTIFIER:
            break;
        case NODE_IF_STATEMENT:
            process_if_statement(gen, node);
            break;
        case NODE_WHILE_LOOP:
            process_while_loop(gen, node);
            break;
        case NODE_ROUND_LOOP:
            process_round_loop(gen, node);
            break;
        case NODE_BLOCK:
            {
                int prev_scope = gen->current_scope_is_global;
                gen->block_level++;
                gen->current_scope_is_global = 0;
            for (size_t i = 0; i < node->block.children.size; i++) {
                process_node(gen, node->block.children.items[i]);
                }
                gen->current_scope_is_global = prev_scope;
                gen->block_level--;
            }
            break;
        case NODE_PRINT:
            process_print(gen, node);
            break;
        default:
            snprintf(buffer, sizeof(buffer), "Warning: Unknown node type %d", node->type);
            add_output(gen, buffer);
            break;
    }
}

static int check_division_by_zero(RISCGenerator *gen, ASTNode *left, ASTNode *right, const char *op) {
    int line = get_current_line();
    int column = get_current_column();
    if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        if (right->type == NODE_LITERAL && 
            strcmp(right->literal.type, "int") == 0 && 
            right->literal.int_value == 0) {
            error_report(ERROR_DIVISION_BY_ZERO, line, column, gen->current_file,
                        "Division by zero detected at compile-time");
            return 1;
        }
        if (right->type == NODE_IDENTIFIER) {
            for (size_t i = 0; i < gen->var_count; i++) {
                if (strcmp(gen->variables[i].name, right->identifier.name) == 0) {
                    int var_addr = get_variable_address(gen, right->identifier.name);
                    if (var_addr != -1) {
                        fprintf(stderr, "Warning: Potential division by zero at %s:%d:%d: Check variable '%s'\n",
                                gen->current_file, line, column, right->identifier.name);
                    }
                    break;
                }
            }
        }
        add_output(gen, "Adding runtime division by zero check");
        return 0;
    }
    return 0;
}

static void evaluate_expression(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    char buffer[128];
    int line = get_current_line();
    int column = get_current_column();
    switch (node->type) {
        case NODE_BINARY_OPERATION:
            if (strcmp(node->binary_op.op_type, ".") == 0) {
                if (node->binary_op.left->type == NODE_BINARY_OPERATION &&
                    strcmp(node->binary_op.left->binary_op.op_type, ".") == 0) {
                    evaluate_expression(gen, node->binary_op.left, "x10");
                    add_output(gen, "addi x5, x10, 0");
                } else {
                    evaluate_expression(gen, node->binary_op.left, "x5");
                }
                evaluate_expression(gen, node->binary_op.right, "x6");
                process_concat(gen, node, target_reg);
            } else {
                const char *left_type = NULL;
                const char *right_type = NULL;
                if (node->binary_op.left->type == NODE_LITERAL) {
                    left_type = node->binary_op.left->literal.type;
                } else if (node->binary_op.left->type == NODE_IDENTIFIER) {
                    for (size_t i = 0; i < gen->var_count; i++) {
                        if (strcmp(gen->variables[i].name, 
                                node->binary_op.left->identifier.name) == 0) {
                            left_type = gen->variables[i].type;
                            break;
                        }
                    }
                }
                if (node->binary_op.right->type == NODE_LITERAL) {
                    right_type = node->binary_op.right->literal.type;
                } else if (node->binary_op.right->type == NODE_IDENTIFIER) {
                    for (size_t i = 0; i < gen->var_count; i++) {
                        if (strcmp(gen->variables[i].name, 
                                node->binary_op.right->identifier.name) == 0) {
                            right_type = gen->variables[i].type;
                            break;
                        }
                    }
                }
                if (left_type && right_type) {
                    error_check_type_compatibility(
                        left_type, right_type, 
                        node->binary_op.op_type, 0, 0, gen->current_file);
                }
                if (strcmp(node->binary_op.op_type, "/") == 0 || strcmp(node->binary_op.op_type, "%") == 0) {
                    if (node->binary_op.right->type == NODE_LITERAL &&
                        strcmp(node->binary_op.right->literal.type, "int") == 0 &&
                        node->binary_op.right->literal.int_value == 0) {
                        error_report(ERROR_DIVISION_BY_ZERO, line, column, gen->current_file,
                                    "Division by zero detected at compile-time");
                        snprintf(buffer, sizeof(buffer), "li %s, 0", target_reg);
                        add_output(gen, buffer);
                        return;
                    }
                }
                char left_reg[8], right_reg[8];
                if (strcmp(target_reg, "x1") == 0) {
                    strcpy(left_reg, "x3");
                    strcpy(right_reg, "x4");
                } else {
                    strcpy(left_reg, "x1");
                    strcpy(right_reg, "x2");
                }
                evaluate_expression(gen, node->binary_op.left, left_reg);
                evaluate_expression(gen, node->binary_op.right, right_reg);
                const char *op = node->binary_op.op_type;
                if (!error_is_critical()) {
                    if (strcmp(op, "+") == 0) {
                        snprintf(buffer, sizeof(buffer), "add %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "-") == 0) {
                        snprintf(buffer, sizeof(buffer), "sub %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "*") == 0) {
                        snprintf(buffer, sizeof(buffer), "mul %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "/") == 0) {
                        add_output(gen, "Check for division by zero at runtime");
                        snprintf(buffer, sizeof(buffer), "beq %s, x0, __division_by_zero_%d", 
                                right_reg, gen->label_counter);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "div %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "jal x0, __after_division_%d", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "__division_by_zero_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "li %s, 0", 
                                target_reg);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "__after_division_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        gen->label_counter++;
                    } else if (strcmp(op, "%") == 0) {
                        if ((left_type && strcmp(left_type, "string") == 0) || 
                            (right_type && strcmp(right_type, "string") == 0)) {
                            error_report(ERROR_TYPE_MISMATCH, line, column, gen->current_file,
                                "Modulo operation requires integer operands, got %s and %s",
                                left_type ? left_type : "unknown", right_type ? right_type : "unknown");
                            snprintf(buffer, sizeof(buffer), "Ошибка: операция модуля применима только к целым числам");
                            add_output(gen, buffer);
                            error_set_critical();
                            return;
                        }
                        add_output(gen, "Check for modulo by zero at runtime");
                        snprintf(buffer, sizeof(buffer), "beq %s, x0, __modulo_by_zero_%d", 
                                right_reg, gen->label_counter);
                        add_output(gen, buffer);
                        add_output(gen, "Compute modulo using rem instruction (remainder)");
                        snprintf(buffer, sizeof(buffer), "rem %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "jal x0, __after_modulo_%d", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "__modulo_by_zero_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "li %s, 0", 
                                target_reg);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "__after_modulo_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        gen->label_counter++;
                    } else if (strcmp(op, "<") == 0) {
                        snprintf(buffer, sizeof(buffer), "slt %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, ">") == 0) {
                        snprintf(buffer, sizeof(buffer), "slt %s, %s, %s", 
                                target_reg, right_reg, left_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
                        add_output(gen, "Equality comparison (==)");
                        snprintf(buffer, sizeof(buffer), "seq %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "!=") == 0) {
                        add_output(gen, "Inequality comparison (!=)");
                        snprintf(buffer, sizeof(buffer), "sne %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "<=") == 0) {
                        add_output(gen, "Less or equal comparison (<=)");
                        snprintf(buffer, sizeof(buffer), "slt x5, %s, %s", 
                                right_reg, left_reg);
                        add_output(gen, buffer);
                        snprintf(buffer, sizeof(buffer), "xori %s, x5, 1", 
                                target_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, ">=") == 0) {
                        add_output(gen, "Greater or equal comparison (>=)");
                        snprintf(buffer, sizeof(buffer), "sge %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "and") == 0) {
                        add_output(gen, "Logical AND (optimized)");
                        if (node->binary_op.left->type == NODE_LITERAL && 
                            strcmp(node->binary_op.left->literal.type, "int") == 0 &&
                            node->binary_op.left->literal.int_value == 0) {
                            snprintf(buffer, sizeof(buffer), "li %s, 0", target_reg);
                            add_output(gen, buffer);
                        } else if (node->binary_op.right->type == NODE_LITERAL && 
                                   strcmp(node->binary_op.right->literal.type, "int") == 0 &&
                                   node->binary_op.right->literal.int_value == 0) {
                            snprintf(buffer, sizeof(buffer), "li %s, 0", target_reg);
                            add_output(gen, buffer);
                        } else {
                            snprintf(buffer, sizeof(buffer), "sne x5, %s, x0", left_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "sne x6, %s, x0", right_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "and %s, x5, x6", target_reg);
                            add_output(gen, buffer);
                        }
                    } else if (strcmp(op, "or") == 0) {
                        add_output(gen, "Logical OR (optimized)");
                        if ((node->binary_op.left->type == NODE_LITERAL && 
                             strcmp(node->binary_op.left->literal.type, "int") == 0 &&
                             node->binary_op.left->literal.int_value != 0) ||
                            (node->binary_op.right->type == NODE_LITERAL && 
                             strcmp(node->binary_op.right->literal.type, "int") == 0 &&
                             node->binary_op.right->literal.int_value != 0)) {
                            snprintf(buffer, sizeof(buffer), "li %s, 1", target_reg);
                            add_output(gen, buffer);
                        } else {
                            snprintf(buffer, sizeof(buffer), "sne x5, %s, x0", left_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "sne x6, %s, x0", right_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "or %s, x5, x6", target_reg);
                            add_output(gen, buffer);
                        }
                    } else {
                        snprintf(buffer, sizeof(buffer), "Warning: Unknown operation %s", op);
                        add_output(gen, buffer);
                    }
                }
            }
            break;
        case NODE_LITERAL:
            if (strcmp(node->literal.type, "int") == 0) {
                snprintf(buffer, sizeof(buffer), "li %s, %d", target_reg, node->literal.int_value);
                add_output(gen, buffer);
            } else if (strcmp(node->literal.type, "string") == 0) {
                int str_id = add_string_literal(gen, node->literal.string_value);
                snprintf(buffer, sizeof(buffer), "li %s, %d", target_reg, str_id);
                add_output(gen, buffer);
            }
            break;
        case NODE_IDENTIFIER:
            {
                int var_addr = get_variable_address(gen, node->identifier.name);
                if (var_addr != -1) {
                    snprintf(buffer, sizeof(buffer), "li x2, %d", 
                            var_addr, node->identifier.name);
            add_output(gen, buffer);
                    add_output(gen, "lw x31, x2, 0");
                    snprintf(buffer, sizeof(buffer), "add %s, x31, x0", target_reg);
            add_output(gen, buffer);
                } else {
                    snprintf(buffer, sizeof(buffer), "li %s, 0", 
                            target_reg);
            add_output(gen, buffer);
        }
            }
            break;
        default:
            snprintf(buffer, sizeof(buffer), "Warning: Unknown expression type %d", node->type);
            add_output(gen, buffer);
            break;
    }
}

static void process_concat(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    char buffer[128];
    evaluate_expression(gen, node->binary_op.left, "x5");
    evaluate_expression(gen, node->binary_op.right, "x6");
    int new_addr = concatenate_strings(gen, "x5", "x6");
    snprintf(buffer, sizeof(buffer), "li %s, %d", target_reg, new_addr);
    add_output(gen, buffer);
}

static int add_string_literal(RISCGenerator *gen, const char *str) {
    if (!str) return -1;
    int str_addr = gen->memory_pos;
    char buffer[128];
    const char *start = str;
    if (*start == '"') start++;
    snprintf(buffer, sizeof(buffer), "li x2, %d", str_addr);
    add_output(gen, buffer);
    for (const char *p = start; *p && *p != '"'; p++) {
        snprintf(buffer, sizeof(buffer), "li x1, %d", *p, *p);
        add_output(gen, buffer);
        add_output(gen, "sw x2, 0, x1");
        add_output(gen, "addi x2, x2, 1");
    }
    add_output(gen, "li x1, 0");
    add_output(gen, "sw x2, 0, x1");
    gen->memory_pos = str_addr + strlen(str) + 10;
    return str_addr;
}

static int is_string_variable(RISCGenerator *gen, const char *name) {
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0) {
            return strcmp(gen->variables[i].type, "string") == 0;
        }
    }
    return 0;
}

static void process_print(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node || !node->print.expression) {
        add_output(gen, "Warning: Invalid print statement");
        return;
    }
    int line = get_current_line();
    int column = get_current_column();
    char buffer[128];
    if (node->print.expression->type == NODE_IDENTIFIER) {
        const char *var_name = node->print.expression->identifier.name;
        int var_addr = get_variable_address(gen, var_name);
        if (var_addr == -1) {
            add_output(gen, "Error: Cannot print undefined or inaccessible variable");
            return;
        }
    }
    evaluate_expression(gen, node->print.expression, "x1");
    add_output(gen, "Print value");
    char *producer_loop_label = get_new_label(gen, "producer_loop");
    char *after_minus_label = get_new_label(gen, "after_minus");
    if ((node->print.expression->type == NODE_LITERAL && 
         strcmp(node->print.expression->literal.type, "string") == 0) ||
        (node->print.expression->type == NODE_IDENTIFIER && 
         is_string_variable(gen, node->print.expression->identifier.name))) {
        char *print_loop_label = get_new_label(gen, "print_loop");
        char *print_done_label = get_new_label(gen, "print_done");
        snprintf(buffer, sizeof(buffer), "%s:", print_loop_label);
        add_output(gen, buffer);
    add_output(gen, "lw x2, x1, 0");
        snprintf(buffer, sizeof(buffer), "beq x2, x0, %s", print_done_label);
        add_output(gen, buffer);
    add_output(gen, "ewrite x2");
    add_output(gen, "addi x1, x1, 1");
        snprintf(buffer, sizeof(buffer), "jal x0, %s", print_loop_label);
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "%s:", print_done_label);
        add_output(gen, buffer);
        free(print_loop_label);
        free(print_done_label);
    } else {
        add_output(gen, "addi x10, x0, 10");
        add_output(gen, "addi x11, x0, 1023");
        add_output(gen, "addi x12, x1, 0");
        add_output(gen, "addi x13, x0, 0");
        add_output(gen, "addi x14, x11, 0");
        snprintf(buffer, sizeof(buffer), "bge x12, x0, %s", producer_loop_label);
        add_output(gen, buffer);
        add_output(gen, "addi x13, x0, 1");
        add_output(gen, "sub x12, x0, x12");
        snprintf(buffer, sizeof(buffer), "%s:", producer_loop_label);
        add_output(gen, buffer);
        add_output(gen, "div x15, x12, x10");
        add_output(gen, "rem x16, x12, x10");
        add_output(gen, "addi x31, x16, 48");
        add_output(gen, "sw x14, 0, x31");
        add_output(gen, "addi x14, x14, -1");
        add_output(gen, "addi x12, x15, 0");
        snprintf(buffer, sizeof(buffer), "bne x12, x0, %s", producer_loop_label);
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "beq x13, x0, %s", after_minus_label);
        add_output(gen, buffer);
        add_output(gen, "addi x31, x0, 45");
        add_output(gen, "ewrite x31");
        snprintf(buffer, sizeof(buffer), "%s:", after_minus_label);
        add_output(gen, buffer);
        add_output(gen, "addi x14, x14, 1");
        add_output(gen, "lw x31, x14, 0");
        add_output(gen, "ewrite x31");
        snprintf(buffer, sizeof(buffer), "bne x14, x11, %s", after_minus_label);
        add_output(gen, buffer);
    }
    add_output(gen, "li x2, 10");
    add_output(gen, "ewrite x2");
    free(producer_loop_label);
    free(after_minus_label);
}

static void process_assignment(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;
    char buffer[128];
    const char *target = node->assignment.target;
    int var_addr = get_variable_address(gen, target);
    if (var_addr == -1) {
        return;
    }
    if (!error_is_critical()) {
        snprintf(buffer, sizeof(buffer), "Assignment to %s", target);
    add_output(gen, buffer);
        evaluate_expression(gen, node->assignment.value, "x1");
        snprintf(buffer, sizeof(buffer), "li x2, %d", var_addr, target);
            add_output(gen, buffer);
        add_output(gen, "sw x2, 0, x1");
    }
}

static int concatenate_strings(RISCGenerator *gen, const char *reg1, const char *reg2) {
    char buffer[128];
    int result_addr = gen->memory_pos;
    char* first_loop_label = get_new_label(gen, "copy_first");
    char* second_loop_label = get_new_label(gen, "copy_second");
    char* first_done_label = get_new_label(gen, "first_done");
    char* second_done_label = get_new_label(gen, "second_done");
    add_output(gen, "String concatenation");
    snprintf(buffer, sizeof(buffer), "add x3, %s, x0", reg1);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "add x4, %s, x0", reg2);
    add_output(gen, buffer);
    add_output(gen, "Copy first string");
    snprintf(buffer, sizeof(buffer), "li x2, %d", result_addr);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", first_loop_label);
    add_output(gen, buffer);
    add_output(gen, "lw x1, x3, 0");
    add_output(gen, "sw x2, 0, x1");
    add_output(gen, "addi x2, x2, 1");
    add_output(gen, "addi x3, x3, 1");
    snprintf(buffer, sizeof(buffer), "lw x1, x3, 0");
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "bne x1, x0, %s", first_loop_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", first_done_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", second_loop_label);
    add_output(gen, buffer);
    add_output(gen, "lw x1, x4, 0");
    add_output(gen, "sw x2, 0, x1");
    snprintf(buffer, sizeof(buffer), "beq x1, x0, %s", second_done_label);
    add_output(gen, buffer);
    add_output(gen, "addi x2, x2, 1");
    add_output(gen, "addi x4, x4, 1");
    snprintf(buffer, sizeof(buffer), "jal x0, %s", second_loop_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", second_done_label);
    add_output(gen, buffer);
    gen->memory_pos = result_addr + 100;
    free(first_loop_label);
    free(second_loop_label);
    free(first_done_label);
    free(second_done_label);
    return result_addr;
}

static void process_if_statement(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;
    char buffer[1024];
    char *else_label = get_new_label(gen, "else");
    char *end_label = get_new_label(gen, "endif");
    add_output(gen, "Begin if-statement");
    evaluate_expression(gen, node->if_stmt.condition, "x1");
    snprintf(buffer, sizeof(buffer), "beq x1, x0, %s", else_label);
    add_output(gen, buffer);
    int prev_scope = gen->current_scope_is_global;
    int prev_block_level = gen->block_level;
    gen->block_level++;
    gen->current_scope_is_global = 0;
    process_node(gen, node->if_stmt.then_branch);
    gen->current_scope_is_global = prev_scope;
    gen->block_level = prev_block_level;
    snprintf(buffer, sizeof(buffer), "jal x0, %s", end_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", else_label);
    add_output(gen, buffer);
    if (node->if_stmt.else_branch) {
        int prev_scope_else = gen->current_scope_is_global;
        int prev_block_level_else = gen->block_level;
        gen->block_level++;
        gen->current_scope_is_global = 0;
        process_node(gen, node->if_stmt.else_branch);
        gen->current_scope_is_global = prev_scope_else;
        gen->block_level = prev_block_level_else;
    }
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);
    add_output(gen, "End if-statement");
    free(else_label);
    free(end_label);
}

static void process_while_loop(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;
    char buffer[1024];
    char *loop_label = get_new_label(gen, "while");
    char *end_label = get_new_label(gen, "endwhile");
    add_output(gen, "Begin while-loop");
    snprintf(buffer, sizeof(buffer), "%s:", loop_label);
    add_output(gen, buffer);
    evaluate_expression(gen, node->while_loop.condition, "x1");
    snprintf(buffer, sizeof(buffer), "beq x1, x0, %s", end_label);
    add_output(gen, buffer);
    int prev_scope = gen->current_scope_is_global;
    int prev_block_level = gen->block_level;
    gen->block_level++;
    gen->current_scope_is_global = 0;
    process_node(gen, node->while_loop.body);
    gen->current_scope_is_global = prev_scope;
    gen->block_level = prev_block_level;
    snprintf(buffer, sizeof(buffer), "jal x0, %s", loop_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);
    add_output(gen, "End while-loop");
    free(loop_label);
    free(end_label);
}

static void process_round_loop(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;
    char buffer[1024];
    char *var_name = node->round_loop.variable;
    int var_addr = get_variable_address(gen, var_name);
    char *loop_label = get_new_label(gen, "round");
    char *end_label = get_new_label(gen, "endround");
    char *body_label = get_new_label(gen, "body");
    add_output(gen, "Begin round loop");
    evaluate_expression(gen, node->round_loop.start, "x1");
    add_output(gen, "add x20, x1, x0");
    snprintf(buffer, sizeof(buffer), "li x2, %d", var_addr);
    add_output(gen, buffer);
    add_output(gen, "sw x2, 0, x20");
    evaluate_expression(gen, node->round_loop.end, "x1");
    add_output(gen, "add x21, x1, x0");
    if (node->round_loop.step) {
        evaluate_expression(gen, node->round_loop.step, "x1");
        add_output(gen, "add x22, x1, x0");
    } else {
        add_output(gen, "addi x22, x0, 1");
    }
    snprintf(buffer, sizeof(buffer), "jal x0, %s", loop_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", body_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "li x2, %d", var_addr);
    add_output(gen, buffer);
    add_output(gen, "sw x2, 0, x20");
    int prev_scope = gen->current_scope_is_global;
    int prev_block_level = gen->block_level;
    gen->block_level++;
    gen->current_scope_is_global = 0;
    process_node(gen, node->round_loop.body);
    gen->current_scope_is_global = prev_scope;
    gen->block_level = prev_block_level;
    add_output(gen, "Increment loop variable in dedicated register");
    add_output(gen, "add x20, x20, x22");
    snprintf(buffer, sizeof(buffer), "li x2, %d", var_addr);
    add_output(gen, buffer);
    add_output(gen, "sw x2, 0, x20");
    snprintf(buffer, sizeof(buffer), "%s:", loop_label);
    add_output(gen, buffer);
    add_output(gen, "Check loop condition using dedicated registers");
    add_output(gen, "slt x5, x20, x21");
    snprintf(buffer, sizeof(buffer), "bne x5, x0, %s", body_label);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);
    add_output(gen, "End round loop");
    free(loop_label);
    free(end_label);
    free(body_label);
}

char *generate_risc_code(ASTNode *ast_root) {
    if (!ast_root) return NULL;
    if (error_is_critical()) {
        fprintf(stderr, "Critical errors found. Code generation aborted.\n");
        return NULL;
    }
    RISCGenerator *gen = init_generator(current_filename);
    if (!gen) return NULL;
    process_node(gen, ast_root);
    if (error_is_critical()) {
        fprintf(stderr, "Critical errors found during code generation. Output aborted.\n");
        free_generator(gen);
        return NULL;
    }
    add_output(gen, "Exit program");
    add_output(gen, "ebreak");
    size_t total_length = 0;
    for (size_t i = 0; i < gen->output_size; i++) {
        total_length += strlen(gen->output[i]) + 1;
    }
    char *result = (char *) malloc(total_length + 1);
    if (!result) {
        free_generator(gen);
        return NULL;
    }
    result[0] = '\0';
    for (size_t i = 0; i < gen->output_size; i++) {
        strcat(result, gen->output[i]);
        strcat(result, "\n");
    }
    free_generator(gen);
    return result;
}

void free_risc_code(char *code) {
    free(code);
} 
