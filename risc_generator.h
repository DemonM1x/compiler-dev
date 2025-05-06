#ifndef RISC_GENERATOR_H
#define RISC_GENERATOR_H

#include "ast.h"

// Генерация RISC-кода из AST
char *generate_risc_code(ASTNode *ast_root);


// Освобождение памяти, выделенной для RISC-кода
void free_risc_code(char *code);

#endif /* RISC_GENERATOR_H */ 