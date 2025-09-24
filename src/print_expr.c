#include "print_expr.h"

static unsigned int num_length(unsigned int val) {
	unsigned int len = 0;
	do {
		len++;
		val /= 10;
	} while (val > 0);
	return len;
}

static void write_num(unsigned int val, char ** str) {
	unsigned int len = num_length(val);
	char * str_pos = *str + len;
	*str = str_pos;
	do {
		str_pos--;
		*str_pos = val % 10 + '0';
		val /= 10;
	} while (val > 0);
}

void dbg_print_expr_size(struct expr expr, size_t * size, int parens) {
	if (expr.lam_count > 0) {
		if (parens & 1) {
			*size += 2;
		}
		*size += num_length(expr.lam_count) + 7;
		parens = 0;
	}
	if (expr.node == NULL) {
		*size += 4;
	} else if (expr_node_is_bvar(expr.node)) {
		*size += 1;
		int num = expr_node_get_bvar(expr.node);
		*size += num_length(num);
	} else if (expr_node_is_op(expr.node)) {
		*size += 1;
		int num = expr_node_get_op(expr.node);
		*size += num_length(num);
	} else {
		if (parens & 2) {
			*size += 2;
		}
		*size += 1;
		dbg_print_expr_size(expr.node->fn, size, 1);
		dbg_print_expr_size(expr.node->arg, size, parens & 2 ? 2 : parens | 2);
	}
}

void dbg_print_expr(struct expr expr, char ** out, int parens) {
	if (expr.lam_count > 0) {
		if (parens & 1) {
			*(*out)++ = '(';
		}
		*(*out)++ = 'l';
		*(*out)++ = 'a';
		*(*out)++ = 'm';
		if (expr.lam_count > 1) {
			*(*out)++ = '[';
			write_num(expr.lam_count, out);
			*(*out)++ = ']';
		}
		*(*out)++ = ' ';
		dbg_print_expr((struct expr) { .node = expr.node, .lam_count = 0 }, out, 0);
		if (parens & 1) {
			*(*out)++ = ')';
		}
		return;
	}
	if (expr.node == NULL) {
		*(*out)++ = 'N';
		*(*out)++ = 'U';
		*(*out)++ = 'L';
		*(*out)++ = 'L';
	} else if (expr_node_is_bvar(expr.node)) {
		*(*out)++ = '#';
		int num = expr_node_get_bvar(expr.node);
		write_num(num, out);
	} else if (expr_node_is_op(expr.node)) {
		*(*out)++ = '!';
		int num = expr_node_get_op(expr.node);
		write_num(num, out);
	} else {
		if (parens & 2) {
			*(*out)++ = '(';
		}
		dbg_print_expr(expr.node->fn, out, 1);
		*(*out)++ = ' ';
		dbg_print_expr(expr.node->arg, out, parens & 2 ? 2 : parens | 2);
		if (parens & 2) {
			*(*out)++ = ')';
		}
	}
}

char * dbg_expr_to_str(struct expr expr) {
	size_t sz = 0;
	dbg_print_expr_size(expr, &sz, 0);
	char * buffer = malloc(sz + 1);
	char * pos = buffer;
	dbg_print_expr(expr, &pos, 0);
	*pos = 0;
	return buffer;
}
