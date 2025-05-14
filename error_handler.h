#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdio.h>

// Типы ошибок
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

// Структура для хранения информации об ошибке
typedef struct {
    ErrorType type;
    int line;
    int column;
    char message[256];
    char filename[256];
} Error;

// Инициализация обработчика ошибок
void error_init(void);

// Освобождение ресурсов обработчика ошибок
void error_free(void);

// Регистрация новой ошибки
void error_report(ErrorType type, int line, int column, const char *filename, const char *format, ...);

// Проверка наличия ошибок
int error_has_errors(void);

// Получение количества ошибок
int error_count(void);

// Вывод всех ошибок
void error_print_all(FILE *output);

// Очистка списка ошибок
void error_clear(void);

// Проверка деления на ноль (на этапе компиляции, если значение известно)
int error_check_division_by_zero(int divisor, int line, int column, const char *filename);

// Проверка определения переменной
int error_check_variable_defined(const char *name, int line, int column, const char *filename);

// Проверка совместимости типов
int error_check_type_compatibility(const char *type1, const char *type2, const char *operation, 
                                  int line, int column, const char *filename);

// Проверка операций со строками
int error_check_string_operation(const char *operation, int line, int column, const char *filename);

// Проверка области видимости
int error_check_variable_scope(const char *name, int is_global, int current_scope_is_global,
                              int line, int column, const char *filename);

// Флаг критической ошибки, который блокирует генерацию кода
int error_is_critical(void);

// Установить критическую ошибку для прерывания компиляции
void error_set_critical(void);

#endif /* ERROR_HANDLER_H */ 