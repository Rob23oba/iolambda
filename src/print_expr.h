#ifndef PRINT_EXPR_H
#define PRINT_EXPR_H

#include "expr.h"

void dbg_print_expr_size(struct expr expr, size_t * size, int parens);
void dbg_print_expr(struct expr expr, char ** out, int parens);
char * dbg_expr_to_str(struct expr expr);

#endif
