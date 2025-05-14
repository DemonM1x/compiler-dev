#ifndef AST_VISUALIZER_H
#define AST_VISUALIZER_H

#include "ast.h"
#include <stdio.h>

/**
 * Визуализирует AST в текстовом виде в указанный файл
 * @param node Корень AST
 * @param output Файл для вывода (например, stdout)
 */
void visualize_ast(ASTNode *node, FILE *output);

/**
 * Визуализирует AST в текстовом виде и сохраняет в файл
 * @param node Корень AST
 * @param filename Имя файла для вывода
 * @return 0 при успехе, -1 при ошибке
 */
int save_ast_to_file(ASTNode *node, const char *filename);

#endif /* AST_VISUALIZER_H */ 