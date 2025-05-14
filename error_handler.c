#include "error_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Прототипы функций из парсера
extern int get_current_line(void);
extern int get_current_column(void);
extern const char* get_parser_filename(void);

#define MAX_ERRORS 3

// Global array for storing errors
static Error errors[MAX_ERRORS];
static int error_count_value = 0;
static int initialized = 0;
static int critical_error = 0; // Critical error flag

// Current scope (0 - local, 1 - global)
static int current_scope_is_global = 1;

// Symbol table for checking variable definitions
typedef struct {
    char *name;
    char *type;
    int is_global;
    int defined;
} SymbolEntry;

#define MAX_SYMBOLS 1000
static SymbolEntry symbols[MAX_SYMBOLS];
static int symbol_count = 0;

void error_init(void) {
    if (!initialized) {
        error_count_value = 0;
        symbol_count = 0;
        memset(errors, 0, sizeof(errors));
        memset(symbols, 0, sizeof(symbols));
        initialized = 1;
    }
}

void error_free(void) {
    if (initialized) {
        // Free memory for symbols
        for (int i = 0; i < symbol_count; i++) {
            free(symbols[i].name);
            free(symbols[i].type);
        }
        error_count_value = 0;
        symbol_count = 0;
        initialized = 0;
    }
}

void error_report(ErrorType type, int line, int column, const char *file, const char *format, ...) {
    va_list args;
    va_start(args, format);

    // Если строка и столбец не указаны, пытаемся получить их из парсера
    if (line == 0 && column == 0) {
        line = get_current_line();
        column = get_current_column();
    }
    
    // Если файл не указан, пытаемся получить его из парсера
    if (file == NULL || strcmp(file, "unknown") == 0) {
        file = get_parser_filename();
    }

    switch (type) {
        case ERROR_UNDEFINED_VARIABLE:
            fprintf(stderr, "Error: Undefined variable in %s:%d:%d: ", file, line, column);
            break;
        case ERROR_TYPE_MISMATCH:
            fprintf(stderr, "Error: Type mismatch in %s:%d:%d: ", file, line, column);
            break;
        case ERROR_DIVISION_BY_ZERO:
            fprintf(stderr, "Error: Division by zero in %s:%d:%d: ", file, line, column);
            break;
        case ERROR_SCOPE:
            fprintf(stderr, "Error: Scope violation in %s:%d:%d: ", file, line, column);
            break;
        case ERROR_REDECLARATION:
            fprintf(stderr, "Error: Variable redeclaration in %s:%d:%d: ", file, line, column);
            break;
        default:
            fprintf(stderr, "Error: Unknown error in %s:%d:%d: ", file, line, column);
            break;
    }

    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);

    // Устанавливаем флаг критической ошибки для определенных типов ошибок
    if (type == ERROR_UNDEFINED_VARIABLE || 
        type == ERROR_TYPE_MISMATCH || 
        type == ERROR_DIVISION_BY_ZERO ||
        type == ERROR_SCOPE ||
        type == ERROR_REDECLARATION) {
        error_set_critical();
    }

    // Увеличиваем счетчик ошибок
    error_count_value++;
}

int error_has_errors(void) {
    return error_count_value > 0;
}

int error_count(void) {
    return error_count_value;
}

void error_clear(void) {
    error_count_value = 0;
}

// Print all errors
void error_print_all(FILE *output) {
    if (error_count_value == 0) {
        fprintf(stderr, "No errors detected.\n");
        return;
    }

    fprintf(stderr, "%d compilation errors detected.\n", error_count_value);
}

// Functions for working with symbol table
static int add_symbol(const char *name, const char *type, int is_global, int defined) {
    if (symbol_count >= MAX_SYMBOLS) {
        return -1;
    }

    // Check if the symbol already exists
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) {
            // If symbol already exists, update its state
            symbols[i].defined = defined;
            return i;
        }
    }

    // Add new symbol
    SymbolEntry *entry = &symbols[symbol_count];
    entry->name = strdup(name);
    entry->type = strdup(type);
    entry->is_global = is_global;
    entry->defined = defined;

    return symbol_count++;
}

static SymbolEntry *find_symbol(const char *name) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) {
            return &symbols[i];
        }
    }
    return NULL;
}

// Set current scope
void set_current_scope(int is_global) {
    current_scope_is_global = is_global;
}

// Declare variable
int declare_variable(const char *name, const char *type, int is_global, int line, int column, const char *filename) {
    SymbolEntry *existing = find_symbol(name);
    
    // If variable is already declared in the same scope, it's a redeclaration error
    if (existing && existing->is_global == is_global) {
        error_report(ERROR_REDECLARATION, line, column, filename, 
                    "Variable '%s' already declared in this scope", name);
        return -1;
    }
    
    // Add variable to symbol table
    int idx = add_symbol(name, type, is_global, 1);
    if (idx < 0) {
        error_report(ERROR_UNKNOWN, line, column, filename, 
                    "Table of symbols limit exceeded");
        return -1;
    }
    
    return idx;
}

// Check division by zero
int error_check_division_by_zero(int divisor, int line, int column, const char *filename) {
    if (divisor == 0) {
        error_report(ERROR_DIVISION_BY_ZERO, line, column, filename, "Division by zero");
        return 1;
    }
    return 0;
}

// Check if variable is defined
int error_check_variable_defined(const char *name, int line, int column, const char *filename) {
    SymbolEntry *entry = find_symbol(name);
    if (!entry) {
        error_report(ERROR_UNDEFINED_VARIABLE, line, column, filename, 
                    "Variable '%s' is not defined", name);
        return 0;
    }
    return 1;
}

// Check type compatibility
int error_check_type_compatibility(const char *type1, const char *type2, const char *operation, 
                                int line, int column, const char *filename) {
    // If both types are the same, they are compatible
    if (strcmp(type1, type2) == 0) {
        // For strings, check operation validity
        if (strcmp(type1, "string") == 0) {
            return error_check_string_operation(operation, line, column, filename);
        }
        
        // Для модуля, проверяем, что тип - числовой
        if (strcmp(operation, "%") == 0) {
            if (strcmp(type1, "int") == 0) {
                return 1;
            } else {
                error_report(ERROR_TYPE_MISMATCH, line, column, filename, 
                            "Operation '%s' requires integer operands", operation);
                critical_error = 1;
                return 0;
            }
        }
        
        return 1;
    }
    
    // Check for string concatenation operation
    if (strcmp(operation, ".") == 0) {
        if (strcmp(type1, "string") == 0 && strcmp(type2, "string") == 0) {
            return 1;
        }
    }
    
    // Types are incompatible
    error_report(ERROR_TYPE_MISMATCH, line, column, filename, 
                "Incompatible types: '%s' and '%s' for operation '%s'", 
                type1, type2, operation);
    critical_error = 1; // Set critical error flag
    return 0;
}

// Check string operations
int error_check_string_operation(const char *operation, int line, int column, const char *filename) {
    // Valid string operations
    if (strcmp(operation, ".") == 0 || strcmp(operation, "=") == 0) {
        return 1; // Concatenation and assignment are allowed
    }
    
    // Other operations are not allowed
    error_report(ERROR_INVALID_STRING_OP, line, column, filename, 
                "Invalid operation '%s' for strings", operation);
    return 0;
}

// Check variable scope
int error_check_variable_scope(const char *name, int is_global, int current_scope_is_global,
                             int line, int column, const char *filename) {
    SymbolEntry *entry = find_symbol(name);
    if (!entry) {
        return 0; // Variable not found (this error is already handled in error_check_variable_defined)
    }
    
    // If current scope is local and variable is global - it's fine
    if (!current_scope_is_global && entry->is_global) {
        return 1;
    }
    
    // If scopes match - it's fine
    if (entry->is_global == is_global) {
        return 1;
    }
    
    // Local variable is not visible in global context
    error_report(ERROR_SCOPE, line, column, filename, 
                "Variable '%s' is not accessible in the current scope", name);
    return 0;
}

// Critical error flag
int error_is_critical(void) {
    return critical_error || error_count_value > 0;
}

// Set critical error
void error_set_critical(void) {
    critical_error = 1;
} 