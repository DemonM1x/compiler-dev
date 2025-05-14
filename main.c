#include <stdio.h>
#include <string.h>
#include "ast/ast.h"
#include "compiler/risc_generator.h"
#include "ast/ast_visualizer.h"
#include "error_handler.h"

// Объявления функций из parser
extern int parser_init(const char *filename);

extern ASTNode *get_ast_root();

void show_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s <file> [options]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <file>    Save RISC code to file\n");
    fprintf(stderr, "  -ast         Show AST\n");
    fprintf(stderr, "  -ast-file <file>  Save AST to file\n");
}

int main(int argc, char **argv) {
    // Настройка кодировки консоли

    // Проверка аргументов командной строки
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *output_file = NULL;
    const char *ast_output_file = NULL;
    int show_ast = 0;

    // Обработка опций командной строки
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-ast") == 0) {
            show_ast = 1;
        } else if (strcmp(argv[i], "-ast-file") == 0 && i + 1 < argc) {
            ast_output_file = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            show_usage(argv[0]);
            return 1;
        }
    }

    // Инициализация обработчика ошибок
    error_init();

    // Инициализация парсера и получение AST
    if (parser_init(filename) != 0) {
        fprintf(stderr, "Error parsing file %s\n", filename);
        error_free();
        return 1;
    }

    // Получение корневого узла AST
    ASTNode *ast_root = get_ast_root();
    if (!ast_root) {
        fprintf(stderr, "Failed to build AST for file %s\n", filename);
        error_free();
        return 1;
    }

    // Показать AST, если запрошено
    if (show_ast) {
        printf("#AST:\n");
        visualize_ast(ast_root, stdout);
        printf("\n");
    }

    // Сохранить AST в файл, если запрошено
    if (ast_output_file) {
        if (save_ast_to_file(ast_root, ast_output_file) != 0) {
            fprintf(stderr, "Error saving AST to file %s\n", ast_output_file);
        } else {
            printf("AST saved to file %s\n", ast_output_file);
        }
    }

    // Проверка наличия ошибок после компиляции
    if (error_has_errors()) {
        fprintf(stderr, "\nCompilation aborted due to errors.\n");
        error_print_all(stderr);
        error_free();
        return 1;
    }

    // Устанавливаем имя файла в риск-генераторе
    set_risc_generator_filename(filename);

    // Генерация RISC-кода из AST
    char *risc_code = generate_risc_code(ast_root);
    if (!risc_code) {
        fprintf(stderr, "Error generating RISC code\n");
        error_free();
        return 1;
    }

    // Вывод сгенерированного кода
    printf("#RISC-code:\n%s\n", risc_code);

    // Сохранение RISC-кода в файл, если запрошено
    if (output_file) {
        FILE *fp = fopen(output_file, "w");
        if (fp) {
            fprintf(fp, "%s", risc_code);
            fclose(fp);
            printf("RISC code saved to file %s\n", output_file);
        } else {
            fprintf(stderr, "Failed to open file %s for writing\n", output_file);
        }
    }

    // Освобождение памяти
    free_risc_code(risc_code);
    error_free();

    return 0;
} 