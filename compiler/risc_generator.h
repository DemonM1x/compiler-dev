#ifndef RISC_GENERATOR_H
#define RISC_GENERATOR_H

#include "ast.h"

// Generate RISC code from AST
char *generate_risc_code(ASTNode *ast_root);

// Free memory allocated for RISC code
void free_risc_code(char *code);

// Set the name of the current source file
void set_risc_generator_filename(const char *filename);

#endif /* RISC_GENERATOR_H */ 