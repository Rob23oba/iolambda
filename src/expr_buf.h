#ifndef EXPR_BUF_H
#define EXPR_BUF_H

#include "expr.h"

struct expr_buf {
	struct expr * data; // owned
	size_t capacity;
	size_t size;
	size_t off;
};

struct expr_buf expr_buf_new();

void expr_buf_extend_front(struct expr_buf * buf, size_t n);

// owned e
void expr_buf_push(struct expr_buf * buf, struct expr e);

#endif
