#ifndef PARSER_H
#define PARSER_H

#include "expr.h"

typedef struct {
	char *str;
	size_t len;
} string_slice;

int string_slice_eq(string_slice a, string_slice b);

struct parser {
	char * fname;
	char * str;
	char * end;
	char * line_start;
	int line_number;
	int error_count;
};

enum token_type {
	TOKEN_ERROR,
	TOKEN_EOF,
	TOKEN_IDENT,
	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_SEP,
	TOKEN_LAMBDA,
};

struct token {
	enum token_type type;
	string_slice value;
};

void parser_init(struct parser * parser, char * fname, char * str, size_t len);
struct expr parse_expr(struct parser * parser);

#endif
