#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdio.h>

typedef enum {
    ERROR_NONE = 0,
    ERROR_SYNTAX,               // Синтаксическая ошибка
    
    ERROR_DIVISION_BY_ZERO,     // Деление на ноль
    ERROR_UNDEFINED_VARIABLE,   // Попытка использовать неопределенную переменную
    ERROR_TYPE_MISMATCH,        // Несовместимые типы данных
    ERROR_INVALID_STRING_OP,    // Недопустимая операция со строкой
    ERROR_SCOPE,                // Ошибка области видимости
    ERROR_UNKNOWN,              // Неизвестная ошибка
    ERROR_REDECLARATION         // Переопределение переменной
} ErrorType;

typedef struct {
    ErrorType type;
    int line;
    int column;
    char message[256];
    char filename[256];
} Error;

void error_init(void);

void error_free(void);

void error_report(ErrorType type, int line, int column, const char *filename, const char *format, ...);

int error_has_errors(void);

int error_count(void);

void error_print_all(FILE *output);

void error_clear(void);

int error_check_division_by_zero(int divisor, int line, int column, const char *filename);

int error_check_variable_defined(const char *name, int line, int column, const char *filename);

int error_check_type_compatibility(const char *type1, const char *type2, const char *operation, 
                                  int line, int column, const char *filename);

int error_check_string_operation(const char *operation, int line, int column, const char *filename);

int error_check_variable_scope(const char *name, int is_global, int current_scope_is_global,
                              int line, int column, const char *filename);

int error_is_critical(void);

void error_set_critical(void);

#endif /* ERROR_HANDLER_H */ 