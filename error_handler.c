#include "error_handler.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern int get_current_line(void);
extern int get_current_column(void);
extern const char* get_parser_filename(void);

#define MAX_ERRORS 3

static Error errors[MAX_ERRORS];
static int error_count_value = 0;
static int initialized = 0;
static int critical_error = 0;

static int current_scope_is_global = 1;

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

    if (line == 0 && column == 0) {
        line = get_current_line();
        column = get_current_column();
    }
    
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

void error_print_all(FILE *output) {
    if (error_count_value == 0) {
        fprintf(stderr, "No errors detected.\n");
        return;
    }

    fprintf(stderr, "%d compilation errors detected.\n", error_count_value);
}

static int add_symbol(const char *name, const char *type, int is_global, int defined) {
    if (symbol_count >= MAX_SYMBOLS) {
        return -1;
    }

    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) {
            symbols[i].defined = defined;
            return i;
        }
    }

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

void set_current_scope(int is_global) {
    current_scope_is_global = is_global;
}

int declare_variable(const char *name, const char *type, int is_global, int line, int column, const char *filename) {
    SymbolEntry *existing = find_symbol(name);
    
    if (existing && existing->is_global == is_global) {
        error_report(ERROR_REDECLARATION, line, column, filename, 
                    "Variable '%s' already declared in this scope", name);
        return -1;
    }
    
    int idx = add_symbol(name, type, is_global, 1);
    if (idx < 0) {
        error_report(ERROR_UNKNOWN, line, column, filename, 
                    "Table of symbols limit exceeded");
        return -1;
    }
    
    return idx;
}

int error_check_division_by_zero(int divisor, int line, int column, const char *filename) {
    if (divisor == 0) {
        error_report(ERROR_DIVISION_BY_ZERO, line, column, filename, "Division by zero");
        return 1;
    }
    return 0;
}

int error_check_variable_defined(const char *name, int line, int column, const char *filename) {
    SymbolEntry *entry = find_symbol(name);
    if (!entry) {
        error_report(ERROR_UNDEFINED_VARIABLE, line, column, filename, 
                    "Variable '%s' is not defined", name);
        return 0;
    }
    return 1;
}

int error_check_type_compatibility(const char *type1, const char *type2, const char *operation, 
                                int line, int column, const char *filename) {
    if (strcmp(type1, type2) == 0) {
        if (strcmp(type1, "string") == 0) {
            return error_check_string_operation(operation, line, column, filename);
        }
        
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
    
    if (strcmp(operation, ".") == 0) {
        if (strcmp(type1, "string") == 0 && strcmp(type2, "string") == 0) {
            return 1;
        }
    }
    
    // Types are incompatible
    error_report(ERROR_TYPE_MISMATCH, line, column, filename, 
                "Incompatible types: '%s' and '%s' for operation '%s'", 
                type1, type2, operation);
    critical_error = 1;
    return 0;
}

int error_check_string_operation(const char *operation, int line, int column, const char *filename) {
    if (strcmp(operation, ".") == 0 || strcmp(operation, "=") == 0) {
        return 1;
    }
    
    error_report(ERROR_INVALID_STRING_OP, line, column, filename, 
                "Invalid operation '%s' for strings", operation);
    return 0;
}

int error_check_variable_scope(const char *name, int is_global, int current_scope_is_global,
                             int line, int column, const char *filename) {
    SymbolEntry *entry = find_symbol(name);
    if (!entry) {
        return 0;
    }
    
    if (!current_scope_is_global && entry->is_global) {
        return 1;
    }
    
    if (entry->is_global == is_global) {
        return 1;
    }
    
    error_report(ERROR_SCOPE, line, column, filename, 
                "Variable '%s' is not accessible in the current scope", name);
    return 0;
}

int error_is_critical(void) {
    return critical_error || error_count_value > 0;
}

void error_set_critical(void) {
    critical_error = 1;
} 