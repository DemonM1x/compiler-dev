#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "risc_generator.h"
#include "ast.h"

// Structure for RISC code generator
typedef struct {
    char **output;        // Output strings array
    size_t output_size;   // Number of strings
    size_t output_capacity; // Capacity of strings array

    // Dictionary for storing variables
    struct {
        char *name;
        char *type;
        int is_constant;
    } *variables;
    size_t var_count;
    size_t var_capacity;

    int temp_counter;     // Counter for temporary variables
    int label_counter;    // Counter for labels
    int data_counter;     // Counter for data in memory

    // Memory addresses for variables (starting from 1000)
    int memory_pos;
    char **data_section;
    size_t data_size;
    size_t data_capacity;
    struct {
        char *name;
        int address;
    } *var_addresses;
    size_t addr_count;
    size_t addr_capacity;
} RISCGenerator;

// Generator initialization
static RISCGenerator *init_generator() {
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

    gen->memory_pos = 1000;
    gen->var_addresses = NULL;
    gen->addr_count = 0;
    gen->addr_capacity = 0;

    return gen;
}

// Free generator memory
static void free_generator(RISCGenerator *gen) {
    if (!gen) return;

    // Free output strings array
    if (gen->output) {
        for (size_t i = 0; i < gen->output_size; i++) {
            free(gen->output[i]);
        }
        free(gen->output);
    }

    // Free variables
    if (gen->variables) {
        for (size_t i = 0; i < gen->var_count; i++) {
            free(gen->variables[i].name);
            free(gen->variables[i].type);
        }
        free(gen->variables);
    }

    // Free variable addresses
    if (gen->var_addresses) {
        for (size_t i = 0; i < gen->addr_count; i++) {
            free(gen->var_addresses[i].name);
        }
        free(gen->var_addresses);
    }

    free(gen);
}

// Add line to output
static void add_output(RISCGenerator *gen, const char *line) {
    if (!gen || !line) return;

    // Check if capacity needs to be increased
    if (gen->output_size >= gen->output_capacity) {
        size_t new_capacity = gen->output_capacity == 0 ? 16 : gen->output_capacity * 2;
        char **new_output = (char **) realloc(gen->output, new_capacity * sizeof(char *));
        if (!new_output) return;

        gen->output = new_output;
        gen->output_capacity = new_capacity;
    }

    // Copy the string
    gen->output[gen->output_size] = strdup(line);
    if (!gen->output[gen->output_size]) return;

    gen->output_size++;
}

// Get variable address
static int get_variable_address(RISCGenerator *gen, const char *name) {
    if (!gen || !name) return -1;

    // Search for variable
    for (size_t i = 0; i < gen->addr_count; i++) {
        if (strcmp(gen->var_addresses[i].name, name) == 0) {
            return gen->var_addresses[i].address;
        }
    }

    // Register new variable
    if (gen->addr_count >= gen->addr_capacity) {
        size_t new_capacity = gen->addr_capacity == 0 ? 8 : gen->addr_capacity * 2;
        void *new_addresses = realloc(gen->var_addresses,
                                      new_capacity * sizeof(*gen->var_addresses));
        if (!new_addresses) return -1;

        gen->var_addresses = new_addresses;
        gen->addr_capacity = new_capacity;
    }

    gen->var_addresses[gen->addr_count].name = strdup(name);
    if (!gen->var_addresses[gen->addr_count].name) return -1;

    gen->var_addresses[gen->addr_count].address = gen->memory_pos;
    int addr = gen->memory_pos;
    gen->memory_pos += 4;
    gen->addr_count++;

    return addr;
}

// Create new temporary variable
static char *get_new_temp(RISCGenerator *gen) {
    if (!gen) return NULL;

    char temp_name[32];
    snprintf(temp_name, sizeof(temp_name), "__tmp%d", gen->temp_counter++);

    // Register temporary variable
    get_variable_address(gen, temp_name);

    return strdup(temp_name);
}

// Create new label
static char *get_new_label(RISCGenerator *gen, const char *prefix) {
    if (!gen || !prefix) return NULL;

    char label_name[64];
    snprintf(label_name, sizeof(label_name), "__%s_%d", prefix, gen->label_counter++);

    return strdup(label_name);
}

// Register variable
static void register_variable(RISCGenerator *gen, const char *name, const char *type, int is_constant) {
    if (!gen || !name) return;

    // Check if capacity needs to be increased
    if (gen->var_count >= gen->var_capacity) {
        size_t new_capacity = gen->var_capacity == 0 ? 8 : gen->var_capacity * 2;
        void *new_vars = realloc(gen->variables, new_capacity * sizeof(*gen->variables));
        if (!new_vars) return;

        gen->variables = new_vars;
        gen->var_capacity = new_capacity;
    }

    gen->variables[gen->var_count].name = strdup(name);
    gen->variables[gen->var_count].type = type ? strdup(type) : strdup("unknown");
    gen->variables[gen->var_count].is_constant = is_constant;
    gen->var_count++;

    // Also register address
    get_variable_address(gen, name);
}

// Check if variable is constant
static int is_variable_constant(RISCGenerator *gen, const char *name) {
    if (!gen || !name) return 0;

    // Все переменные не являются константами
    return 0;
}

// Function declarations for AST processing
static void process_node(RISCGenerator *gen, ASTNode *node);

static void evaluate_expression(RISCGenerator *gen, ASTNode *node, const char *target_reg);

static void process_binary_operation(RISCGenerator *gen, ASTNode *node, const char *target_reg);

static void process_if_statement(RISCGenerator *gen, ASTNode *node);

static void process_while_loop(RISCGenerator *gen, ASTNode *node);

static void process_round_loop(RISCGenerator *gen, ASTNode *node);

static void process_print(RISCGenerator *gen, ASTNode *node);

static int add_string_literal(RISCGenerator *gen, const char *str);

// Process AST node
static void process_node(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) {
        add_output(gen, "# Warning: NULL node detected!");
        return;
    }

    char buffer[1024];

    switch (node->type) {
        case NODE_PROGRAM:
            for (size_t i = 0; i < node->block.children.size; i++) {
                process_node(gen, node->block.children.items[i]);
            }
            break;

        case NODE_VARIABLE_DECLARATION: {
            char *name = node->variable.name;
            char *type = node->variable.var_type;
            int is_global = node->variable.is_global;

            // Register variable - все переменные не константы
            register_variable(gen, name, type, 0);

            // Get variable address
            int var_addr = get_variable_address(gen, name);

            snprintf(buffer, sizeof(buffer), "# Variable declaration %s (type: %s)",
                     name, type ? type : "unknown");
            add_output(gen, buffer);

            if (node->variable.initializer) {
                snprintf(buffer, sizeof(buffer), "# Initialize %s with expression", name);
                add_output(gen, buffer);

                evaluate_expression(gen, node->variable.initializer, "x1");
            } else {
                snprintf(buffer, sizeof(buffer), "li x1, 0  # Initialize %s with 0", name);
                add_output(gen, buffer);
            }

            snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, name);
            add_output(gen, buffer);

            add_output(gen, "sw x2, 0, x1  # Store initial value");
            break;
        }

        case NODE_ASSIGNMENT: {
            char *target = node->assignment.target;

            // Get variable address
            int var_addr = get_variable_address(gen, target);

            // Generate code for expression
            snprintf(buffer, sizeof(buffer), "# Assignment to %s", target);
            add_output(gen, buffer);

            // Evaluate expression to register x1
            evaluate_expression(gen, node->assignment.value, "x1");

            // Load address to register x2
            snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, target);
            add_output(gen, buffer);

            add_output(gen, "sw x2, 0, x1  # Store value to variable");
            break;
        }

        case NODE_BINARY_OPERATION:
            // Processed in evaluate_expression
            break;

        case NODE_LITERAL:
            // Processed in evaluate_expression
            break;

        case NODE_IDENTIFIER:
            // Processed in evaluate_expression
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
            for (size_t i = 0; i < node->block.children.size; i++) {
                process_node(gen, node->block.children.items[i]);
            }
            break;

        case NODE_PRINT:
            process_print(gen, node);
            break;

        default:
            snprintf(buffer, sizeof(buffer), "# Warning: Unknown node type %d", node->type);
            add_output(gen, buffer);
            break;
    }
}

// Evaluate expression
static void evaluate_expression(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    if (!gen || !node || !target_reg) return;

    char buffer[1024];

    switch (node->type) {
        case NODE_BINARY_OPERATION:
            process_binary_operation(gen, node, target_reg);
            break;

        case NODE_LITERAL:
            if (strcmp(node->literal.type, "int") == 0) {
                snprintf(buffer, sizeof(buffer), "li %s, %d", target_reg, node->literal.int_value);
                add_output(gen, buffer);
            } else if (strcmp(node->literal.type, "float") == 0) {
                // Handle float as int for simplicity
                snprintf(buffer, sizeof(buffer), "li %s, %d",
                         target_reg, (int) node->literal.float_value);
                add_output(gen, buffer);
            } else if (strcmp(node->literal.type, "string") == 0) {
                // Strings have limited support
                snprintf(buffer, sizeof(buffer), "# Literal: \"%s\" (strings have limited support)",
                         node->literal.string_value);
                add_output(gen, buffer);
                add_output(gen, "li x0, 0"); // Load 0 into target_reg
            } else if (strcmp(node->literal.type, "string") == 0) {
                int str_id = add_string_literal(gen, node->literal.string_value);
                snprintf(buffer, sizeof(buffer), "la %s, __str_%d", target_reg, str_id);
                add_output(gen, buffer);
            }
            break;

        case NODE_IDENTIFIER: {
            char *name = node->identifier.name;
            int addr = get_variable_address(gen, name);

            snprintf(buffer, sizeof(buffer), "# Load variable %s", name);
            add_output(gen, buffer);

            // Load address to register x5
            snprintf(buffer, sizeof(buffer), "li x5, %d", addr);
            add_output(gen, buffer);

            // Правильный синтаксис: lw rd, rs1, imm[12]
            // rd - регистр назначения, rs1 - базовый адрес, imm[12] - смещение
            snprintf(buffer, sizeof(buffer), "lw %s, x5, 0  # Load from memory", target_reg);
            add_output(gen, buffer);
            break;
        }

        default:
            snprintf(buffer, sizeof(buffer), "# Warning: Unknown expression type %d", node->type);
            add_output(gen, buffer);
            break;
    }
}

// Process binary operation
static void process_binary_operation(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    if (!gen || !node || !target_reg) return;

    char buffer[1024];
    const char *op = node->binary_op.op_type;

    // Evaluate left operand to register x1
    evaluate_expression(gen, node->binary_op.left, "x1");

    // Evaluate right operand to register x2
    evaluate_expression(gen, node->binary_op.right, "x2");

    // Perform operation
    if (strcmp(op, "+") == 0) {
        snprintf(buffer, sizeof(buffer), "add %s, x1, x2", target_reg);
        add_output(gen, buffer);
    } else if (strcmp(op, "-") == 0) {
        snprintf(buffer, sizeof(buffer), "sub %s, x1, x2", target_reg);
        add_output(gen, buffer);
    } else if (strcmp(op, "*") == 0) {
        snprintf(buffer, sizeof(buffer), "mul %s, x1, x2", target_reg);
        add_output(gen, buffer);
    } else if (strcmp(op, "/") == 0) {
        snprintf(buffer, sizeof(buffer), "div %s, x1, x2", target_reg);
        add_output(gen, buffer);
    } else if (strcmp(op, "<") == 0) {
        snprintf(buffer, sizeof(buffer), "# Less than comparison");
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "slt %s, x1, x2", target_reg);
        add_output(gen, buffer);
    } else if (strcmp(op, ">") == 0) {
        snprintf(buffer, sizeof(buffer), "# Greater than comparison");
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "slt %s, x2, x1", target_reg);
        add_output(gen, buffer);
    } else if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
        snprintf(buffer, sizeof(buffer), "# Equality comparison");
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "sub x3, x1, x2");
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "seq %s, x3, x0", target_reg);
        add_output(gen, buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "# Warning: Unknown operation %s", op);
        add_output(gen, buffer);
    }
}

static int add_string_literal(RISCGenerator *gen, const char *str) {
    if (!gen || !str) return -1;
    if (gen->data_size >= gen->data_capacity) {
        size_t new_capacity = gen->data_capacity == 0 ? 8 : gen->data_capacity * 2;
        char **new_data = (char **) realloc(gen->data_section, new_capacity * sizeof(char *));
        if (!new_data) return -1;
        gen->data_section = new_data;
        gen->data_capacity = new_capacity;
    }
    char label[64];
    snprintf(label, sizeof(label), "__str_%zu", gen->data_size);
    size_t len = strlen(str);
    char *data_line = (char *) malloc(len * 8 + 128);
    if (!data_line) return -1;
    strcpy(data_line, label);
    strcat(data_line, ": data ");
    for (size_t i = 0; i < len; i++) {
        char num[16];
        snprintf(num, sizeof(num), "%d ", (unsigned char) str[i]);
        strcat(data_line, num);
    }
    strcat(data_line, "0");
    gen->data_section[gen->data_size] = data_line;
    gen->data_size++;
    return (int) (gen->data_size - 1);
}

// Process if statement
static void process_if_statement(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;

    char buffer[1024];

    // Create labels
    char *else_label = get_new_label(gen, "else");
    char *end_label = get_new_label(gen, "endif");

    add_output(gen, "# Begin if-statement");

    // Evaluate condition to register x1
    evaluate_expression(gen, node->if_stmt.condition, "x1");

    // If condition is false (x1 == 0), jump to else label
    snprintf(buffer, sizeof(buffer), "beq x1, x0, %s  # Jump if condition is false", else_label);
    add_output(gen, buffer);

    // Generate code for then branch
    process_node(gen, node->if_stmt.then_branch);

    // Jump to end of if-statement
    snprintf(buffer, sizeof(buffer), "jal x0, %s", end_label);
    add_output(gen, buffer);

    // Else label
    snprintf(buffer, sizeof(buffer), "%s:", else_label);
    add_output(gen, buffer);

    // Generate code for else branch if exists
    if (node->if_stmt.else_branch) {
        process_node(gen, node->if_stmt.else_branch);
    }

    // End if-statement label
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);

    add_output(gen, "# End if-statement");

    free(else_label);
    free(end_label);
}

// Process while loop
static void process_while_loop(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;

    char buffer[1024];

    // Create labels
    char *loop_label = get_new_label(gen, "while");
    char *end_label = get_new_label(gen, "endwhile");

    add_output(gen, "# Begin while-loop");

    // Loop start label
    snprintf(buffer, sizeof(buffer), "%s:", loop_label);
    add_output(gen, buffer);

    // Evaluate condition to register x1
    evaluate_expression(gen, node->while_loop.condition, "x1");

    // If condition is false (x1 == 0), exit loop
    snprintf(buffer, sizeof(buffer), "beq x1, x0, %s  # Exit loop if condition is false", end_label);
    add_output(gen, buffer);

    // Generate code for loop body
    process_node(gen, node->while_loop.body);

    // Jump back to loop start
    snprintf(buffer, sizeof(buffer), "jal x0, %s", loop_label);
    add_output(gen, buffer);

    // End loop label
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);

    add_output(gen, "# End while-loop");

    free(loop_label);
    free(end_label);
}

// Process round loop
static void process_round_loop(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;

    char buffer[1024];

    // Get loop variable name
    char *var_name = node->round_loop.variable;
    int var_addr = get_variable_address(gen, var_name);

    // Create labels
    char *loop_label = get_new_label(gen, "round");
    char *end_label = get_new_label(gen, "endround");
    char *increment_label = get_new_label(gen, "increment");

    add_output(gen, "# Begin round loop");

    // Evaluate start value and store in loop variable
    evaluate_expression(gen, node->round_loop.start, "x1");
    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    // Правильный синтаксис: sw rs1, imm[12], rs2, где rs1 - базовый адрес, rs2 - данные
    add_output(gen, "sw x2, 0, x1  # Store initial value");

    // Evaluate end value to register x3
    evaluate_expression(gen, node->round_loop.end, "x3");

    // Evaluate step to register x4
    if (node->round_loop.step) {
        evaluate_expression(gen, node->round_loop.step, "x4");
    } else {
        // Default step is 1
        add_output(gen, "addi x4, x0, 1  # Default step = 1");
    }

    // Loop start label
    snprintf(buffer, sizeof(buffer), "%s:", loop_label);
    add_output(gen, buffer);

    // Load loop variable value
    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    // Правильный синтаксис: lw rd, rs1, imm[12]
    add_output(gen, "lw x1, x2, 0  # Load current value");

    // Compare with end value
    add_output(gen, "# Check loop condition");

    // If current value > end value, exit
    // slt x5, x3, x1 (x5 = 1 if end < current)
    add_output(gen, "slt x5, x3, x1  # Check if current > end");
    snprintf(buffer, sizeof(buffer), "bne x5, x0, %s  # Exit if current > end", end_label);
    add_output(gen, buffer);

    // Generate code for loop body
    process_node(gen, node->round_loop.body);

    // Increment loop variable
    snprintf(buffer, sizeof(buffer), "%s:", increment_label);
    add_output(gen, buffer);

    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    // Правильный синтаксис: lw rd, rs1, imm[12]
    add_output(gen, "lw x1, x2, 0  # Load current value");

    add_output(gen, "add x1, x1, x4  # Add step to current value");

    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    // Правильный синтаксис: sw rs1, imm[12], rs2, где rs1 - базовый адрес, rs2 - данные
    add_output(gen, "sw x2, 0, x1  # Store updated value");

    // Jump back to loop start
    snprintf(buffer, sizeof(buffer), "jal x0, %s", loop_label);
    add_output(gen, buffer);

    // End loop label
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);

    add_output(gen, "# End round loop");

    free(loop_label);
    free(end_label);
    free(increment_label);
}

// Process print statement
static void process_print(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;


    char buffer[1024];
    if (node->print.expression->type == NODE_LITERAL && strcmp(node->print.expression->literal.type, "string") == 0) {
        int str_id = add_string_literal(gen, node->print.expression->literal.string_value);
        snprintf(buffer, sizeof(buffer), "la x1, __str_%d", str_id);
        add_output(gen, buffer);
        char *loop_label = get_new_label(gen, "str_print_loop");
        char *end_label = get_new_label(gen, "str_print_end");
        snprintf(buffer, sizeof(buffer), "%s:", loop_label);
        add_output(gen, buffer);
        add_output(gen, "lb x2, x1, 0");
        snprintf(buffer, sizeof(buffer), "beq x2, x0, %s", end_label);
        add_output(gen, buffer);
        add_output(gen, "ewrite x2");
        add_output(gen, "addi x1, x1, 1");
        snprintf(buffer, sizeof(buffer), "jal x0, %s", loop_label);
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "%s:", end_label);
        add_output(gen, buffer);
        add_output(gen, "addi x31, x0, 10");
        add_output(gen, "ewrite x31");
        free(loop_label);
        free(end_label);
        add_output(gen, "# Print string completed");
        return;
    }
    char *end_label = get_new_label(gen, "print_end");
    char *loop_label = get_new_label(gen, "print_loop");
    char *after_minus_label = get_new_label(gen, "after_minus");

    add_output(gen, "# Print value");

    // Evaluate expression to register x1
    evaluate_expression(gen, node->print.expression, "x1");

    add_output(gen, "addi x10, x0, 10 ");
    add_output(gen, "addi x11, x0, 1023 ");
    add_output(gen, "addi x12, x1, 0 ");
    add_output(gen, "addi x13, x0, 0 ");
    add_output(gen, "addi x14, x11, 0 ");

    // Проверяем знак числа
    snprintf(buffer, sizeof(buffer), "bge x12, x0, %s", loop_label);
    add_output(gen, buffer);
    add_output(gen, "addi x13, x0, 1 ");
    add_output(gen, "sub x12, x0, x12 ");

    // Цикл разбиения числа на цифры и сохранения их в буфере
    snprintf(buffer, sizeof(buffer), "%s:", loop_label);
    add_output(gen, buffer);
    add_output(gen, "div x15, x12, x10 ");
    add_output(gen, "rem x16, x12, x10 ");
    add_output(gen, "addi x31, x16, 48 ");
    add_output(gen, "sw x14, 0, x31 ");
    add_output(gen, "addi x14, x14, -1 ");
    add_output(gen, "addi x12, x15, 0 ");
    snprintf(buffer, sizeof(buffer), "bne x12, x0, %s ", loop_label);
    add_output(gen, buffer);

    // Если число отрицательное, выводим минус
    snprintf(buffer, sizeof(buffer), "beq x13, x0, %s", after_minus_label);
    add_output(gen, buffer);
    add_output(gen, "addi x31, x0, 45 ");
    add_output(gen, "ewrite x31 ");

    // Цикл вывода цифр из буфера
    snprintf(buffer, sizeof(buffer), "%s:", after_minus_label);
    add_output(gen, buffer);
    add_output(gen, "addi x14, x14, 1 ");
    // Правильный синтаксис: lw rd, rs1, imm[12]
    add_output(gen, "lw x31, x14, 0 ");
    add_output(gen, "ewrite x31 ");
    snprintf(buffer, sizeof(buffer), "bne x14, x11, %s ", after_minus_label);
    add_output(gen, buffer);

    // Выводим перевод строки
    add_output(gen, "addi x31, x0, 10 ");
    add_output(gen, "ewrite x31 ");

    add_output(gen, "# Print completed");

    free(end_label);
    free(loop_label);
    free(after_minus_label);
}

// Generate RISC code from AST
char *generate_risc_code(ASTNode *ast_root) {
    if (!ast_root) return NULL;

    // Initialize generator
    RISCGenerator *gen = init_generator();
    if (!gen) return NULL;

    // Generate code for AST
    process_node(gen, ast_root);

    // Add program exit
    add_output(gen, "# Exit program");
    add_output(gen, "ebreak  # Stop execution");

    // Form code string
    size_t total_length = 0;
    for (size_t i = 0; i < gen->output_size; i++) {
        total_length += strlen(gen->output[i]) + 1; // +1 for newline
    }

    char *result = (char *) malloc(total_length + 1); // +1 for null terminator
    if (!result) {
        free_generator(gen);
        return NULL;
    }

    result[0] = '\0';
    for (size_t i = 0; i < gen->output_size; i++) {
        strcat(result, gen->output[i]);
        strcat(result, "\n");
    }

    // Free generator memory
    free_generator(gen);

    return result;
}

// Free memory allocated for RISC code
void free_risc_code(char *code) {
    free(code);
} 