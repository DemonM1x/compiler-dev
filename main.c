#include <stdio.h>
#include "ast.h"
#include "risc_generator.h"

// Объявления функций из parser
extern int parser_init(const char *filename);

extern ASTNode *get_ast_root();

int main(int argc, char **argv) {
    // Настройка кодировки консоли

    // Проверка аргументов командной строки
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <файл>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    // Инициализация парсера и получение AST
    if (parser_init(filename) != 0) {
        fprintf(stderr, "Ошибка при парсинге файла %s\n", filename);
        return 1;
    }

    // Получение корневого узла AST
    ASTNode *ast_root = get_ast_root();
    if (!ast_root) {
        fprintf(stderr, "Не удалось построить AST для файла %s\n", filename);
        return 1;
    }

    // Генерация RISC-кода из AST
    char *risc_code = generate_risc_code(ast_root);
    if (!risc_code) {
        fprintf(stderr, "Ошибка при генерации RISC-кода\n");
        return 1;
    }

    // Вывод сгенерированного кода
    printf("#RISC-code:\n%s\n", risc_code);

    // Сохранение кода в файл (опционально)
    if (argc > 2) {
        const char *output_file = argv[2];
        FILE *fp = fopen(output_file, "w");
        if (fp) {
            fprintf(fp, "%s", risc_code);
            fclose(fp);
            printf("Код сохранен в файл %s\n", output_file);
        } else {
            fprintf(stderr, "Не удалось открыть файл %s для записи\n", output_file);
        }
    }

    // Освобождение памяти
    free_risc_code(risc_code);

    return 0;
} 