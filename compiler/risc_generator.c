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

// Structure for RISC code generator
typedef struct {
    char **output;        // Output strings array
    size_t output_size;   // Number of strings
    size_t output_capacity; // Capacity of strings array

    // Dictionary for storing variables
    struct {
        char *name;
        char *type;
        int is_global;    // 1 - evere (глобальная), 0 - lim (локальная)
        int block_level;  // Уровень вложенности блока, в котором объявлена переменная
    } *variables;
    size_t var_count;
    size_t var_capacity;

    int temp_counter;     // Counter for temporary variables
    int label_counter;    // Counter for labels
    int data_counter;     // Counter for data in memory
    int block_level;      // Текущий уровень вложенности блоков

    // Memory addresses for variables (starting from 1000)
    int memory_pos;
    int temp_string_pos;  // Позиция для временных строк
    char **data_section;
    size_t data_size;
    size_t data_capacity;
    struct {
        char *name;
        int address;
    } *var_addresses;
    size_t addr_count;
    size_t addr_capacity;
    
    // Для обработки ошибок
    char current_file[256]; // Имя текущего файла
    
    // Текущая область видимости (1 - глобальная, 0 - локальная)
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
static void process_binary_operation(RISCGenerator *gen, ASTNode *node, const char *target_reg);
static void process_concat(RISCGenerator *gen, ASTNode *node, const char *target_reg);
static int add_string_literal(RISCGenerator *gen, const char *str);
static int concatenate_strings(RISCGenerator *gen, const char *reg1, const char *reg2);
static int is_string_variable(RISCGenerator *gen, const char *name);
static int declare_variable(RISCGenerator *gen, const char *name, const char *type, int is_global);
static int check_division_by_zero(RISCGenerator *gen, ASTNode *left, ASTNode *right, const char *op);

// Глобальная переменная для хранения имени файла
static char current_filename[256] = "unknown";

// Установить имя текущего файла
void set_risc_generator_filename(const char *filename) {
    if (filename) {
        strncpy(current_filename, filename, sizeof(current_filename) - 1);
        current_filename[sizeof(current_filename) - 1] = '\0';
    } else {
        strcpy(current_filename, "unknown");
    }
}

// Generator initialization
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
    gen->block_level = 0;  // Начинаем с нулевого уровня вложенности

    gen->memory_pos = 1000;
    gen->temp_string_pos = 2000;  // Начинаем с 2000 для временных строк
    gen->var_addresses = NULL;
    gen->addr_count = 0;
    gen->addr_capacity = 0;
    
    // Инициализация обработчика ошибок
    if (filename) {
        strncpy(gen->current_file, filename, sizeof(gen->current_file) - 1);
        gen->current_file[sizeof(gen->current_file) - 1] = '\0';
    } else {
        strcpy(gen->current_file, "unknown");
    }
    
    // Начинаем с глобальной области видимости
    gen->current_scope_is_global = 1;
    
    error_init();

    return gen;
}

// Free generator memory
static void free_generator(RISCGenerator *gen) {

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
    // Переменные для хранения текущей позиции
    int line = get_current_line();
    int column = get_current_column();
    
    // Проверяем наличие переменной
    int found = 0;
    int is_global = 0;
    int var_block_level = -1;
    size_t var_index = 0;
    
    // Ищем переменную в таблице переменных
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0) {
            found = 1;
            is_global = gen->variables[i].is_global;
            var_block_level = gen->variables[i].block_level;
            var_index = i;
            break;
        }
    }
    
    // Если переменная не найдена, сообщаем об ошибке
    if (!found) {
        error_report(ERROR_UNDEFINED_VARIABLE, line, column, gen->current_file,
                   "Variable '%s' is not defined", name);
        return -1;
    }
    
    // Проверка области видимости
    if (!is_global) {  // Для локальных переменных
        // Проверка уровня блока
        if (gen->current_scope_is_global) {
            // Локальная переменная не видна в глобальной области
            error_report(ERROR_SCOPE, line, column, gen->current_file,
                       "Cannot access local variable '%s' from global scope", name);
            return -1;
        }
        
        // Проверка доступности переменной в текущем блоке
        // Переменная доступна только если объявлена в текущем или внешнем блоке
        if (var_block_level > gen->block_level) {
            error_report(ERROR_SCOPE, line, column, gen->current_file,
                       "Cannot access variable '%s' outside its declaring block", name);
            return -1;
        }
    }
    
    // Сначала проверяем, есть ли переменная уже в таблице адресов
    for (size_t i = 0; i < gen->addr_count; i++) {
        if (strcmp(gen->var_addresses[i].name, name) == 0) {
            return gen->var_addresses[i].address;
        }
    }

    // Если переменная найдена, но адрес не зарегистрирован, регистрируем его
    return register_variable_address(gen, name);
}

// Register variable address
static int register_variable_address(RISCGenerator *gen, const char *name) {
    if (!gen || !name) return -1;

    // Check if capacity needs to be increased
    if (gen->addr_count >= gen->addr_capacity) {
        size_t new_capacity = gen->addr_capacity == 0 ? 8 : gen->addr_capacity * 2;
        void *new_addrs = realloc(gen->var_addresses, new_capacity * sizeof(*gen->var_addresses));
        if (!new_addrs) return -1;

        gen->var_addresses = new_addrs;
        gen->addr_capacity = new_capacity;
    }

    // Register variable address
    gen->var_addresses[gen->addr_count].name = strdup(name);
    gen->var_addresses[gen->addr_count].address = gen->memory_pos;
    gen->memory_pos += 1;  // Reserve only 1 memory slot for now

    return gen->var_addresses[gen->addr_count++].address;
}

// Create new temporary variable
static char *get_new_temp(RISCGenerator *gen) {
    char temp_name[32];
    snprintf(temp_name, sizeof(temp_name), "__tmp%d", gen->temp_counter++);

    // Register temporary variable as local
    register_variable(gen, temp_name, "int", 0);
    // Сразу регистрируем адрес
    register_variable_address(gen, temp_name);

    return strdup(temp_name);
}

// Create new label
static char *get_new_label(RISCGenerator *gen, const char *prefix) {
    char label_name[64];
    snprintf(label_name, sizeof(label_name), "__%s_%d", prefix, gen->label_counter++);

    return strdup(label_name);
}

// Register variable
static void register_variable(RISCGenerator *gen, const char *name, const char *type, int is_global) {
    if (!name) return;

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
    gen->variables[gen->var_count].is_global = is_global;
    gen->variables[gen->var_count].block_level = is_global ? 0 : gen->block_level;
    gen->var_count++;
}

// Check if variable is constant
static int is_variable_constant(RISCGenerator *gen, const char *name) {
    if (!gen || !name) return 0;

    // Все переменные не являются константами
    return 0;
}

// Declare variable
static int declare_variable(RISCGenerator *gen, const char *name, const char *type, int is_global) {
    if (!gen || !name) return -1;

    // Проверяем, не объявлена ли уже переменная в текущей области видимости
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0 && 
            gen->variables[i].is_global == is_global) {
            error_report(ERROR_REDECLARATION, 0, 0, gen->current_file,
                        "Variable '%s' is already declared in this scope", name);
            return -1;
        }
    }

    // Проверяем, что глобальные переменные объявляются только в глобальной области видимости
    if (is_global && !gen->current_scope_is_global) {
        error_report(ERROR_SCOPE, 0, 0, gen->current_file,
                    "Cannot declare global variable '%s' in local scope", name);
        return -1;
    }

    // Регистрируем переменную в генераторе
    register_variable(gen, name, type, is_global);
    return 0;
}

// Process variable declaration
static void process_variable_declaration(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;

    int line = get_current_line();
    int column = get_current_column();
    
    // Get variable name and type
    const char *name = node->variable.name;
    const char *type = node->variable.var_type;
    int is_global = node->variable.is_global;

    // Проверяем, что глобальные переменные объявляются только в глобальной области видимости
    if (is_global && !gen->current_scope_is_global) {
        error_report(ERROR_SCOPE, line, column, gen->current_file,
                    "Cannot declare global variable '%s' in local scope", name);
        return;
    }

    // Проверяем, не объявлена ли уже переменная в текущей области видимости
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0 && 
            gen->variables[i].is_global == is_global) {
            error_report(ERROR_REDECLARATION, line, column, gen->current_file,
                        "Variable '%s' is already declared in this scope", name);
            return;
        }
    }

    // Регистрируем переменную
    register_variable(gen, name, type, is_global);
    
    // Get variable address
    int var_addr = register_variable_address(gen, name);
    
    // Generate code for initialization (if provided)
    if (node->variable.initializer) {
        char buffer[256];

        // Проверка совместимости типов для инициализатора
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
            snprintf(buffer, sizeof(buffer), "# Initialize %s (address %d)", name, var_addr);
            add_output(gen, buffer);

            evaluate_expression(gen, node->variable.initializer, "x1");

            snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, name);
            add_output(gen, buffer);
            add_output(gen, "sw x2, 0, x1  # Store value to variable");
        }
    } else {
        // Инициализируем переменную нулем по умолчанию
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "# Initialize %s with default value 0 (address %d)", name, var_addr);
        add_output(gen, buffer);
        add_output(gen, "li x1, 0  # Default value");
        snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, name);
        add_output(gen, buffer);
        add_output(gen, "sw x2, 0, x1  # Store default value to variable");
    }
}

// Process AST node
static void process_node(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) {
        add_output(gen, "# Warning: NULL node detected!");
        return;
    }

    char buffer[1024];

    switch (node->type) {
        case NODE_PROGRAM:
            // Глобальная область видимости для программы
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
            {
                // Сохраняем текущую область видимости
                int prev_scope = gen->current_scope_is_global;
                
                // Увеличиваем уровень вложенности блока
                gen->block_level++;
                
                // Входим в локальную область видимости
                gen->current_scope_is_global = 0;
                
            for (size_t i = 0; i < node->block.children.size; i++) {
                process_node(gen, node->block.children.items[i]);
                }
                
                // Восстанавливаем предыдущую область видимости
                gen->current_scope_is_global = prev_scope;
                
                // Уменьшаем уровень вложенности блока
                gen->block_level--;
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

// Проверка деления на ноль
static int check_division_by_zero(RISCGenerator *gen, ASTNode *left, ASTNode *right, const char *op) {
    int line = get_current_line();
    int column = get_current_column();
    
    // Проверка деления на ноль для литералов на этапе компиляции
    if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        if (right->type == NODE_LITERAL && 
            strcmp(right->literal.type, "int") == 0 && 
            right->literal.int_value == 0) {
            
            error_report(ERROR_DIVISION_BY_ZERO, line, column, gen->current_file,
                        "Division by zero detected at compile-time");
            return 1;
        }
        
        // Проверка для переменных с известными значениями
        if (right->type == NODE_IDENTIFIER) {
            // Проверяем, является ли переменная инициализированной константой
            for (size_t i = 0; i < gen->var_count; i++) {
                if (strcmp(gen->variables[i].name, right->identifier.name) == 0) {
                    // Если значение переменной известно на этапе компиляции (например, глобальная с инициализатором)
                    int var_addr = get_variable_address(gen, right->identifier.name);
                    if (var_addr != -1) {
                        // Здесь можно добавить проверку для известных переменных
                        // Например, для переменных, инициализированных нулем
                        // Пока просто выдаем предупреждение
                        fprintf(stderr, "Warning: Potential division by zero at %s:%d:%d: Check variable '%s'\n",
                                gen->current_file, line, column, right->identifier.name);
                    }
                    break;
                }
            }
        }
        
        // Генерируем проверку во время выполнения для всех остальных случаев
        add_output(gen, "# Adding runtime division by zero check");
        return 0;
    }
    
    return 0;
}

// Evaluate expression
static void evaluate_expression(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    char buffer[1024];
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
                // Получаем типы операндов
                const char *left_type = NULL;
                const char *right_type = NULL;
                
                // Определяем тип левого операнда
                if (node->binary_op.left->type == NODE_LITERAL) {
                    left_type = node->binary_op.left->literal.type;
                } else if (node->binary_op.left->type == NODE_IDENTIFIER) {
                    // Ищем тип переменной
                    for (size_t i = 0; i < gen->var_count; i++) {
                        if (strcmp(gen->variables[i].name, 
                                node->binary_op.left->identifier.name) == 0) {
                            left_type = gen->variables[i].type;
                            break;
                        }
                    }
                }
                
                // Определяем тип правого операнда
                if (node->binary_op.right->type == NODE_LITERAL) {
                    right_type = node->binary_op.right->literal.type;
                } else if (node->binary_op.right->type == NODE_IDENTIFIER) {
                    // Ищем тип переменной
                    for (size_t i = 0; i < gen->var_count; i++) {
                        if (strcmp(gen->variables[i].name, 
                                node->binary_op.right->identifier.name) == 0) {
                            right_type = gen->variables[i].type;
                            break;
                        }
                    }
                }
                
                // Проверяем совместимость типов, если оба типа известны
                if (left_type && right_type) {
                    error_check_type_compatibility(
                        left_type, right_type, 
                        node->binary_op.op_type, 0, 0, gen->current_file);
                }
                
                // Проверка операции деления на ноль
                if (strcmp(node->binary_op.op_type, "/") == 0 || strcmp(node->binary_op.op_type, "%") == 0) {
                    // Проверка деления на литерал 0
                    if (node->binary_op.right->type == NODE_LITERAL &&
                        strcmp(node->binary_op.right->literal.type, "int") == 0 &&
                        node->binary_op.right->literal.int_value == 0) {
                        
                        // Сообщаем об ошибке деления на ноль на этапе компиляции
                        error_report(ERROR_DIVISION_BY_ZERO, line, column, gen->current_file,
                                    "Division by zero detected at compile-time");
                        
                        // Возвращаем 0 как результат деления на ноль
                        snprintf(buffer, sizeof(buffer), "li %s, 0  # Division by zero", target_reg);
                        add_output(gen, buffer);
                        return;
                    }
                }
                
                // Сохраняем результаты подвыражений в отдельных регистрах
                char left_reg[8], right_reg[8];
                
                // Если целевой регистр x1, используем x3 и x4 для временных результатов
                // Иначе используем x1 и x2
                if (strcmp(target_reg, "x1") == 0) {
                    strcpy(left_reg, "x3");
                    strcpy(right_reg, "x4");
                } else {
                    strcpy(left_reg, "x1");
                    strcpy(right_reg, "x2");
                }
                
                // Вычисляем левую и правую части выражения
                evaluate_expression(gen, node->binary_op.left, left_reg);
                evaluate_expression(gen, node->binary_op.right, right_reg);
                
                // Выполняем операцию
                const char *op = node->binary_op.op_type;
                
                // Если нет критической ошибки, генерируем код операции
                if (!error_is_critical()) {
                    // Генерация кода операции
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
                        // Добавляем проверку деления на ноль во время выполнения
                        add_output(gen, "# Check for division by zero at runtime");
                        snprintf(buffer, sizeof(buffer), "beq %s, x0, __division_by_zero_%d", 
                                right_reg, gen->label_counter);
                        add_output(gen, buffer);
                        
                        // Если делитель не ноль, выполняем деление
                        snprintf(buffer, sizeof(buffer), "div %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                        
                        // Пропускаем обработку ошибки
                        snprintf(buffer, sizeof(buffer), "jal x0, __after_division_%d", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        
                        // Метка для обработки деления на ноль
                        snprintf(buffer, sizeof(buffer), "__division_by_zero_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        
                        // В случае деления на ноль возвращаем 0
                        snprintf(buffer, sizeof(buffer), "li %s, 0  # Division by zero, returning 0", 
                                target_reg);
                        add_output(gen, buffer);
                        
                        // Метка после обработки деления
                        snprintf(buffer, sizeof(buffer), "__after_division_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        
                        // Увеличиваем счетчик меток
                        gen->label_counter++;
                    } else if (strcmp(op, "%") == 0) {
                        // Проверка, что операнды - целые числа
                        if ((left_type && strcmp(left_type, "string") == 0) || 
                            (right_type && strcmp(right_type, "string") == 0)) {
                            error_report(ERROR_TYPE_MISMATCH, line, column, gen->current_file,
                                "Modulo operation requires integer operands, got %s and %s",
                                left_type ? left_type : "unknown", right_type ? right_type : "unknown");
                            snprintf(buffer, sizeof(buffer), "# Ошибка: операция модуля применима только к целым числам");
                            add_output(gen, buffer);
                            // Устанавливаем критическую ошибку
                            error_set_critical();
                            return;
                        }
                        
                        // Добавляем проверку деления на ноль во время выполнения
                        add_output(gen, "# Check for modulo by zero at runtime");
                        snprintf(buffer, sizeof(buffer), "beq %s, x0, __modulo_by_zero_%d", 
                                right_reg, gen->label_counter);
                        add_output(gen, buffer);
                        
                        // Если делитель не ноль, выполняем операцию модуля
                        add_output(gen, "# Compute modulo using rem instruction (remainder)");
                        snprintf(buffer, sizeof(buffer), "rem %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                        
                        // Пропускаем обработку ошибки
                        snprintf(buffer, sizeof(buffer), "jal x0, __after_modulo_%d", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        
                        // Метка для обработки деления на ноль
                        snprintf(buffer, sizeof(buffer), "__modulo_by_zero_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        
                        // В случае деления на ноль возвращаем 0
                        snprintf(buffer, sizeof(buffer), "li %s, 0  # Modulo by zero, returning 0", 
                                target_reg);
                        add_output(gen, buffer);
                        
                        // Метка после обработки модуля
                        snprintf(buffer, sizeof(buffer), "__after_modulo_%d:", 
                                gen->label_counter);
                        add_output(gen, buffer);
                        
                        // Увеличиваем счетчик меток
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
                        // Используем прямую команду seq для проверки равенства
                        add_output(gen, "# Equality comparison (==)");
                        snprintf(buffer, sizeof(buffer), "seq %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "!=") == 0) {
                        // Используем прямую команду sne для проверки неравенства
                        add_output(gen, "# Inequality comparison (!=)");
                        snprintf(buffer, sizeof(buffer), "sne %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "<=") == 0) {
                        // Для a <= b используем !slt(b, a) = !(b < a) = a <= b
                        add_output(gen, "# Less or equal comparison (<=)");
                        // Сначала вычисляем b < a (если true, то a > b)
                        snprintf(buffer, sizeof(buffer), "slt x5, %s, %s", 
                                right_reg, left_reg);
                        add_output(gen, buffer);
                        // Инвертируем результат: !(b < a) = a <= b
                        snprintf(buffer, sizeof(buffer), "xori %s, x5, 1", 
                                target_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, ">=") == 0) {
                        // Используем прямую команду sge для проверки больше или равно
                        add_output(gen, "# Greater or equal comparison (>=)");
                        snprintf(buffer, sizeof(buffer), "sge %s, %s, %s", 
                                target_reg, left_reg, right_reg);
                        add_output(gen, buffer);
                    } else if (strcmp(op, "and") == 0) {
                        // Логическое И: оптимизированная версия
                        add_output(gen, "# Logical AND (optimized)");
                        
                        // Оптимизация для случая с константами или известными значениями
                        if (node->binary_op.left->type == NODE_LITERAL && 
                            strcmp(node->binary_op.left->literal.type, "int") == 0 &&
                            node->binary_op.left->literal.int_value == 0) {
                            // Если левый операнд равен 0, результат всегда 0
                            snprintf(buffer, sizeof(buffer), "li %s, 0  # Оптимизация: левый операнд = 0, результат AND всегда 0", target_reg);
                            add_output(gen, buffer);
                        } else if (node->binary_op.right->type == NODE_LITERAL && 
                                   strcmp(node->binary_op.right->literal.type, "int") == 0 &&
                                   node->binary_op.right->literal.int_value == 0) {
                            // Если правый операнд равен 0, результат всегда 0
                            snprintf(buffer, sizeof(buffer), "li %s, 0  # Оптимизация: правый операнд = 0, результат AND всегда 0", target_reg);
                            add_output(gen, buffer);
                        } else {
                            // Стандартная реализация AND
                            snprintf(buffer, sizeof(buffer), "sne x5, %s, x0", left_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "sne x6, %s, x0", right_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "and %s, x5, x6", target_reg);
                            add_output(gen, buffer);
                        }
                    } else if (strcmp(op, "or") == 0) {
                        // Логическое ИЛИ: оптимизированная версия
                        add_output(gen, "# Logical OR (optimized)");
                        
                        // Оптимизация для случая с константами или известными значениями
                        if ((node->binary_op.left->type == NODE_LITERAL && 
                             strcmp(node->binary_op.left->literal.type, "int") == 0 &&
                             node->binary_op.left->literal.int_value != 0) ||
                            (node->binary_op.right->type == NODE_LITERAL && 
                             strcmp(node->binary_op.right->literal.type, "int") == 0 &&
                             node->binary_op.right->literal.int_value != 0)) {
                            // Если любой операнд не равен 0, результат всегда 1
                            snprintf(buffer, sizeof(buffer), "li %s, 1  # Оптимизация: ненулевой операнд, результат OR всегда 1", target_reg);
                            add_output(gen, buffer);
                        } else {
                            // Стандартная реализация OR
                            snprintf(buffer, sizeof(buffer), "sne x5, %s, x0", left_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "sne x6, %s, x0", right_reg);
                            add_output(gen, buffer);
                            snprintf(buffer, sizeof(buffer), "or %s, x5, x6", target_reg);
                            add_output(gen, buffer);
                        }
                    } else {
                        snprintf(buffer, sizeof(buffer), "# Warning: Unknown operation %s", op);
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
            // Загружаем значение переменной в регистр
            {
                int var_addr = get_variable_address(gen, node->identifier.name);
                if (var_addr != -1) {
                    // Переменная найдена, загружаем ее значение
                    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", 
                            var_addr, node->identifier.name);
            add_output(gen, buffer);
                    add_output(gen, "lw x31, x2, 0  # Load variable value");
                    snprintf(buffer, sizeof(buffer), "add %s, x31, x0", target_reg);
            add_output(gen, buffer);
                } else {
                    // Переменная не найдена или не в области видимости
                    // Используем значение по умолчанию, но вывод уже был сделан в get_variable_address
                    snprintf(buffer, sizeof(buffer), "li %s, 0  # Default value for undefined/inaccessible variable", 
                            target_reg);
            add_output(gen, buffer);
        }
            }
            break;

        default:
            snprintf(buffer, sizeof(buffer), "# Warning: Unknown expression type %d", node->type);
            add_output(gen, buffer);
            break;
    }
}

// Process binary operation
static void process_binary_operation(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    char buffer[1024];
    const char *op = node->binary_op.op_type;

    if (strcmp(op, ".") == 0) {
        add_output(gen, "# String concatenation");
        evaluate_expression(gen, node->binary_op.left, "x1");
        evaluate_expression(gen, node->binary_op.right, "x2");
        
        // Проверка, что обе части - строки
        if (node->binary_op.left->type == NODE_LITERAL && 
            strcmp(node->binary_op.left->literal.type, "string") != 0) {
            error_check_string_operation(".", 0, 0, gen->current_file);
        }
        
        if (node->binary_op.right->type == NODE_LITERAL && 
            strcmp(node->binary_op.right->literal.type, "string") != 0) {
            error_check_string_operation(".", 0, 0, gen->current_file);
        }
        
        int new_str_addr = concatenate_strings(gen, "x1", "x2");
        snprintf(buffer, sizeof(buffer), "li %s, %d", target_reg, new_str_addr);
        add_output(gen, buffer);
        return;
    }

    // Для других операций используем общую логику в evaluate_expression
    evaluate_expression(gen, node, target_reg);
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

    // Сохраняем текущую область видимости и уровень вложенности
    int prev_scope = gen->current_scope_is_global;
    int prev_block_level = gen->block_level;
    
    // Увеличиваем уровень вложенности блока
    gen->block_level++;
    
    // Устанавливаем локальную область видимости для тела if
    gen->current_scope_is_global = 0;

    // Generate code for then branch
    process_node(gen, node->if_stmt.then_branch);
    
    // Восстанавливаем область видимости и уровень вложенности
    gen->current_scope_is_global = prev_scope;
    gen->block_level = prev_block_level;

    // Jump to end of if-statement
    snprintf(buffer, sizeof(buffer), "jal x0, %s", end_label);
    add_output(gen, buffer);

    // Else label
    snprintf(buffer, sizeof(buffer), "%s:", else_label);
    add_output(gen, buffer);

    // Generate code for else branch if exists
    if (node->if_stmt.else_branch) {
        // Сохраняем текущую область видимости и уровень вложенности
        int prev_scope_else = gen->current_scope_is_global;
        int prev_block_level_else = gen->block_level;
        
        // Увеличиваем уровень вложенности блока
        gen->block_level++;
        
        // Устанавливаем локальную область видимости для тела else
        gen->current_scope_is_global = 0;
        
        process_node(gen, node->if_stmt.else_branch);
        
        // Восстанавливаем область видимости и уровень вложенности
        gen->current_scope_is_global = prev_scope_else;
        gen->block_level = prev_block_level_else;
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

    // Сохраняем текущую область видимости и уровень вложенности
    int prev_scope = gen->current_scope_is_global;
    int prev_block_level = gen->block_level;
    
    // Увеличиваем уровень вложенности блока
    gen->block_level++;
    
    // Устанавливаем локальную область видимости для тела цикла
    gen->current_scope_is_global = 0;

    // Generate code for loop body
    process_node(gen, node->while_loop.body);
    
    // Восстанавливаем область видимости и уровень вложенности
    gen->current_scope_is_global = prev_scope;
    gen->block_level = prev_block_level;

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
    char *body_label = get_new_label(gen, "body");

    add_output(gen, "# Begin round loop");

    // Используем выделенные регистры для хранения переменных цикла:
    // x20 - переменная итератора (i)
    // x21 - конечное значение (end)
    // x22 - шаг (step)

    // Evaluate start value and store in loop variable and x20
    evaluate_expression(gen, node->round_loop.start, "x1");
    add_output(gen, "add x20, x1, x0  # Save iterator to dedicated register");
    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    add_output(gen, "sw x2, 0, x20  # Store initial value");

    // Evaluate end value to register x21
    evaluate_expression(gen, node->round_loop.end, "x1");
    add_output(gen, "add x21, x1, x0  # Save end value to dedicated register");

    // Evaluate step to register x22
    if (node->round_loop.step) {
        evaluate_expression(gen, node->round_loop.step, "x1");
        add_output(gen, "add x22, x1, x0  # Save step to dedicated register");
    } else {
        // Default step is 1
        add_output(gen, "addi x22, x0, 1  # Default step = 1");
    }

    // Jump to condition check
    snprintf(buffer, sizeof(buffer), "jal x0, %s", loop_label);
    add_output(gen, buffer);

    // Loop body label
    snprintf(buffer, sizeof(buffer), "%s:", body_label);
    add_output(gen, buffer);

    // Update loop variable in memory (to make it visible in the body)
    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    add_output(gen, "sw x2, 0, x20  # Update loop variable in memory");

    // Сохраняем текущую область видимости и уровень вложенности
    int prev_scope = gen->current_scope_is_global;
    int prev_block_level = gen->block_level;
    
    // Увеличиваем уровень вложенности блока
    gen->block_level++;
    
    // Устанавливаем локальную область видимости для тела цикла
    gen->current_scope_is_global = 0;

    // Generate code for loop body
    process_node(gen, node->round_loop.body);

    // Восстанавливаем область видимости и уровень вложенности
    gen->current_scope_is_global = prev_scope;
    gen->block_level = prev_block_level;

    // Increment loop iterator (x20) using step (x22)
    add_output(gen, "# Increment loop variable in dedicated register");
    add_output(gen, "add x20, x20, x22  # Add step to iterator");

    // Update loop variable in memory
    snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, var_name);
    add_output(gen, buffer);
    add_output(gen, "sw x2, 0, x20  # Store updated value");

    // Loop condition check label
    snprintf(buffer, sizeof(buffer), "%s:", loop_label);
    add_output(gen, buffer);

    // Check loop condition: if i < end then continue
    add_output(gen, "# Check loop condition using dedicated registers");
    add_output(gen, "slt x5, x20, x21  # x5 = 1 if i < end, 0 otherwise");
    snprintf(buffer, sizeof(buffer), "bne x5, x0, %s  # If i < end, continue loop", body_label);
    add_output(gen, buffer);

    // End loop label
    snprintf(buffer, sizeof(buffer), "%s:", end_label);
    add_output(gen, buffer);

    add_output(gen, "# End round loop");

    free(loop_label);
    free(end_label);
    free(body_label);
}

static int concatenate_strings(RISCGenerator *gen, const char *reg1, const char *reg2) {
    char buffer[256];
    int result_addr = gen->memory_pos;
    char* first_loop_label = get_new_label(gen, "copy_first");
    char* second_loop_label = get_new_label(gen, "copy_second");
    char* first_done_label = get_new_label(gen, "first_done");
    char* second_done_label = get_new_label(gen, "second_done");

    // Сохраняем адреса строк
    add_output(gen, "# String concatenation");
    
    // Копируем регистры в рабочие регистры
    snprintf(buffer, sizeof(buffer), "add x3, %s, x0  # first string", reg1);
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "add x4, %s, x0  # second string", reg2);
    add_output(gen, buffer);

    // Копируем первую строку
    add_output(gen, "# Copy first string");
    snprintf(buffer, sizeof(buffer), "li x2, %d  # result address", result_addr);
    add_output(gen, buffer);
    
    // Цикл копирования первой строки
    snprintf(buffer, sizeof(buffer), "%s:", first_loop_label);
    add_output(gen, buffer);
    add_output(gen, "lw x1, x3, 0  # load char from first string");
    add_output(gen, "sw x2, 0, x1  # save char to result");
    add_output(gen, "addi x2, x2, 1  # next result position");
    add_output(gen, "addi x3, x3, 1  # next source position");
    snprintf(buffer, sizeof(buffer), "lw x1, x3, 0  # check next char");
    add_output(gen, buffer);
    snprintf(buffer, sizeof(buffer), "bne x1, x0, %s  # if not null, continue loop", first_loop_label);
    add_output(gen, buffer);
    
    // Первая строка скопирована, переходим ко второй
    snprintf(buffer, sizeof(buffer), "%s:", first_done_label);
    add_output(gen, buffer);
    
    // Цикл копирования второй строки
    snprintf(buffer, sizeof(buffer), "%s:", second_loop_label);
    add_output(gen, buffer);
    add_output(gen, "lw x1, x4, 0  # load char from second string");
    add_output(gen, "sw x2, 0, x1  # save char to result");
    snprintf(buffer, sizeof(buffer), "beq x1, x0, %s  # if null, second string done", second_done_label);
    add_output(gen, buffer);
    add_output(gen, "addi x2, x2, 1  # next result position");
    add_output(gen, "addi x4, x4, 1  # next source position");
    snprintf(buffer, sizeof(buffer), "jal x0, %s  # loop back", second_loop_label);
    add_output(gen, buffer);
    
    // Конец конкатенации
    snprintf(buffer, sizeof(buffer), "%s:", second_done_label);
    add_output(gen, buffer);

    // Обновляем позицию в памяти
    gen->memory_pos = result_addr + 100; // Резервируем место для строки
    
    free(first_loop_label);
    free(second_loop_label);
    free(first_done_label);
    free(second_done_label);
    
    return result_addr;
}

static void process_concat(RISCGenerator *gen, ASTNode *node, const char *target_reg) {
    char buffer[256];

    // Загружаем выражения для конкатенации
    evaluate_expression(gen, node->binary_op.left, "x5");
    evaluate_expression(gen, node->binary_op.right, "x6");

    // Вызываем функцию конкатенации
    int new_addr = concatenate_strings(gen, "x5", "x6");

    // Сохраняем результат в целевой регистр
    snprintf(buffer, sizeof(buffer), "li %s, %d", target_reg, new_addr);
    add_output(gen, buffer);
}

// Добавление строкового литерала в секцию данных
static int add_string_literal(RISCGenerator *gen, const char *str) {
    if (!str) return -1;

    int str_addr = gen->memory_pos;
    char buffer[256];

    // Пропускаем только начальные кавычки, сохраняем пробелы
    const char *start = str;
    if (*start == '"') start++;

    // Загружаем начальный адрес в регистр
    snprintf(buffer, sizeof(buffer), "li x2, %d  # start address", str_addr);
    add_output(gen, buffer);

    // Сохраняем каждый символ строки в память как ASCII код
    for (const char *p = start; *p && *p != '"'; p++) {
        snprintf(buffer, sizeof(buffer), "li x1, %d  # '%c'", *p, *p);
        add_output(gen, buffer);
        add_output(gen, "sw x2, 0, x1  # save char");
        add_output(gen, "addi x2, x2, 1  # next position");
    }

    // Добавляем нулевой терминатор
    add_output(gen, "li x1, 0  # null terminator");
    add_output(gen, "sw x2, 0, x1");

    // Обновляем позицию в памяти
    gen->memory_pos = str_addr + strlen(str) + 10;  // Увеличиваем отступ
    return str_addr;
}

// Проверка является ли переменная строкой
static int is_string_variable(RISCGenerator *gen, const char *name) {
    for (size_t i = 0; i < gen->var_count; i++) {
        if (strcmp(gen->variables[i].name, name) == 0) {
            return strcmp(gen->variables[i].type, "string") == 0;
        }
    }
    return 0;
}

// Process print statement
static void process_print(RISCGenerator *gen, ASTNode *node) {
    // Проверяем корректность аргументов
    if (!gen || !node || !node->print.expression) {
        add_output(gen, "# Warning: Invalid print statement");
        return;
    }
    
    int line = get_current_line();
    int column = get_current_column();
    char buffer[256];

    // Если выражение - идентификатор, проверяем доступность переменной
    if (node->print.expression->type == NODE_IDENTIFIER) {
        const char *var_name = node->print.expression->identifier.name;
        int var_addr = get_variable_address(gen, var_name);
        
        if (var_addr == -1) {
            // Ошибка уже выведена в get_variable_address
            add_output(gen, "# Error: Cannot print undefined or inaccessible variable");
            return;
        }
    }

    // Загружаем значение для вывода
    evaluate_expression(gen, node->print.expression, "x1");
    add_output(gen, "# Print value");

    // Создаем уникальные метки для операции вывода
    char *producer_loop_label = get_new_label(gen, "producer_loop");
    char *after_minus_label = get_new_label(gen, "after_minus");

    // Проверяем тип выражения
    if ((node->print.expression->type == NODE_LITERAL && 
         strcmp(node->print.expression->literal.type, "string") == 0) ||
        (node->print.expression->type == NODE_IDENTIFIER && 
         is_string_variable(gen, node->print.expression->identifier.name))) {
        // Вывод строки
        char *print_loop_label = get_new_label(gen, "print_loop");
        char *print_done_label = get_new_label(gen, "print_done");
        
        snprintf(buffer, sizeof(buffer), "%s:", print_loop_label);
        add_output(gen, buffer);
    add_output(gen, "lw x2, x1, 0  # load char");
        snprintf(buffer, sizeof(buffer), "beq x2, x0, %s  # check for null", print_done_label);
        add_output(gen, buffer);
    add_output(gen, "ewrite x2  # print char");
    add_output(gen, "addi x1, x1, 1  # next char");
        snprintf(buffer, sizeof(buffer), "jal x0, %s  # continue loop", print_loop_label);
        add_output(gen, buffer);
        snprintf(buffer, sizeof(buffer), "%s:", print_done_label);
        add_output(gen, buffer);
        
        free(print_loop_label);
        free(print_done_label);
    } else {
        // Вывод числа
        add_output(gen, "addi x10, x0, 10  # divisor");
        add_output(gen, "addi x11, x0, 1023  # buffer end");
        add_output(gen, "addi x12, x1, 0  # number to print");
        add_output(gen, "addi x13, x0, 0  # is negative flag");
        add_output(gen, "addi x14, x11, 0  # buffer position");
        snprintf(buffer, sizeof(buffer), "bge x12, x0, %s  # if positive, skip negation", producer_loop_label);
        add_output(gen, buffer);
        add_output(gen, "addi x13, x0, 1  # set negative flag");
        add_output(gen, "sub x12, x0, x12  # negate number");

        snprintf(buffer, sizeof(buffer), "%s:", producer_loop_label);
        add_output(gen, buffer);
        add_output(gen, "div x15, x12, x10  # divide by 10");
        add_output(gen, "rem x16, x12, x10  # get remainder");
        add_output(gen, "addi x31, x16, 48  # convert to ASCII");
        add_output(gen, "sw x14, 0, x31  # store digit");
        add_output(gen, "addi x14, x14, -1  # move buffer position");
        add_output(gen, "addi x12, x15, 0  # update number");
        snprintf(buffer, sizeof(buffer), "bne x12, x0, %s  # continue if not zero", producer_loop_label);
        add_output(gen, buffer);

        snprintf(buffer, sizeof(buffer), "beq x13, x0, %s  # skip minus if positive", after_minus_label);
        add_output(gen, buffer);
        add_output(gen, "addi x31, x0, 45  # minus sign ASCII");
        add_output(gen, "ewrite x31  # print minus");

        snprintf(buffer, sizeof(buffer), "%s:", after_minus_label);
        add_output(gen, buffer);
        add_output(gen, "addi x14, x14, 1  # adjust buffer position");
        add_output(gen, "lw x31, x14, 0  # load digit");
        add_output(gen, "ewrite x31  # print digit");
        snprintf(buffer, sizeof(buffer), "bne x14, x11, %s  # continue if not end", after_minus_label);
        add_output(gen, buffer);
    }

    // Выводим перевод строки
    add_output(gen, "li x2, 10  # newline");
    add_output(gen, "ewrite x2");
    
    // Освобождаем память
    free(producer_loop_label);
    free(after_minus_label);
}

// Process assignment
static void process_assignment(RISCGenerator *gen, ASTNode *node) {
    if (!gen || !node) return;

    char buffer[256];
    char *target = node->assignment.target;
    
    // Получаем адрес переменной (может быть -1, если переменная не найдена или недоступна)
    int var_addr = get_variable_address(gen, target);
    if (var_addr == -1) {
        // Ошибка уже выведена в get_variable_address
        return;
    }

    // Если нет критической ошибки, генерируем код для присваивания
    if (!error_is_critical()) {
        // Generate code for expression
        snprintf(buffer, sizeof(buffer), "# Assignment to %s", target);
    add_output(gen, buffer);

        // Evaluate expression to register x1
        evaluate_expression(gen, node->assignment.value, "x1");

        // Load address to register x2
        snprintf(buffer, sizeof(buffer), "li x2, %d  # Load address of %s", var_addr, target);
            add_output(gen, buffer);
            
        add_output(gen, "sw x2, 0, x1  # Store value to variable");
    }
}

// Generate RISC code from AST
char *generate_risc_code(ASTNode *ast_root) {
    if (!ast_root) return NULL;

    // Check for critical errors before generating code
    if (error_is_critical()) {
        fprintf(stderr, "Critical errors found. Code generation aborted.\n");
        return NULL;
    }

    // Initialize generator with the current filename
    RISCGenerator *gen = init_generator(current_filename);
    if (!gen) return NULL;

    // Generate code for AST
    process_node(gen, ast_root);
    
    // Check if errors occurred during generation
    if (error_is_critical()) {
        fprintf(stderr, "Critical errors found during code generation. Output aborted.\n");
        free_generator(gen);
        return NULL;
    }

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
