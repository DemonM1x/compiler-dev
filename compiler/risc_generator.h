#ifndef RISC_GENERATOR_H
#define RISC_GENERATOR_H

#include "ast.h"

char *generate_risc_code(ASTNode *ast_root);

void free_risc_code(char *code);

void set_risc_generator_filename(const char *filename);

#endif /* RISC_GENERATOR_H */ 