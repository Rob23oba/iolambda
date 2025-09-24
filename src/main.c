#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct expr_node;

struct expr {
	// bound variables are represented as `(struct expr_node *) ((x << 1) | 1)`
	struct expr_node * node;
	unsigned int lam_count; // amount of surrounding lambdas
};

struct expr_node {
	struct expr fn;
	unsigned int rc;
	struct expr arg;
	unsigned int bvar_range;
};

int expr_node_is_bvar(struct expr_node * node) {
	return (((size_t) node) & 3) == 1;
}

int expr_node_is_op(struct expr_node * node) {
	return (((size_t) node) & 3) == 3;
}

int expr_node_is_app(struct expr_node * node) {
	return node != NULL && (((size_t) node) & 1) == 0;
}

unsigned int expr_node_get_bvar(struct expr_node * node) {
	return (unsigned int) (((size_t) node) >> 2);
}

unsigned int expr_node_get_op(struct expr_node * node) {
	return (unsigned int) (((size_t) node) >> 2);
}

unsigned int expr_get_bvar_range(struct expr e) {
	unsigned int inner_range;
	if (expr_node_is_bvar(e.node)) {
		inner_range = expr_node_get_bvar(e.node) + 1;
	} else if (expr_node_is_app(e.node)) {
		inner_range = e.node->bvar_range;
	} else {
		return 0;
	}
	if (inner_range <= e.lam_count) {
		return 0;
	}
	return inner_range - e.lam_count;
}

struct expr_node * mk_bvar_expr_node(unsigned int var_index) {
	return (struct expr_node *) ((((size_t) var_index << 2) | 1));
}

struct expr_node * mk_op_expr_node(unsigned int op_index) {
	return (struct expr_node *) ((((size_t) op_index << 2) | 3));
}

struct expr mk_bvar_expr(unsigned int var_index) {
	return (struct expr) { .node = mk_bvar_expr_node(var_index), .lam_count = 0 };
}

struct expr mk_op_expr(unsigned int op_index) {
	return (struct expr) { .node = mk_op_expr_node(op_index), .lam_count = 0 };
}

struct expr_node * mk_app_expr_node() {
	struct expr_node * node = malloc(sizeof(struct expr_node));
	node->rc = 1;
	return node;
}

// owned fn, owned arg, owned return
struct expr mk_app_expr(struct expr fn, struct expr arg) {
	struct expr_node * node = mk_app_expr_node();
	node->fn = fn;
	node->arg = arg;
	unsigned int fn_range = expr_get_bvar_range(fn);
	unsigned int arg_range = expr_get_bvar_range(arg);
	node->bvar_range = fn_range < arg_range ? arg_range : fn_range;
	return (struct expr) { .node = node, .lam_count = 0 };
}

// owned body, owned return
struct expr mk_lam_expr(struct expr body) {
	return (struct expr) { .node = body.node, .lam_count = body.lam_count + 1 };
}

// owned body, owned return
struct expr mk_multi_lam_expr(struct expr body, unsigned int n) {
	return (struct expr) { .node = body.node, .lam_count = body.lam_count + n };
}

void expr_inc_rc(struct expr e) {
	if (expr_node_is_app(e.node)) {
		e.node->rc++;
	}
}

void expr_dec_rc(struct expr e) {
	if (expr_node_is_app(e.node)) {
		e.node->rc--;
		if (e.node->rc != 0) {
			return;
		}
		struct expr fn = e.node->fn;
		struct expr arg = e.node->arg;
		free(e.node);
		expr_dec_rc(fn);
		expr_dec_rc(arg);
	}
}

// owned e, owned return (with rc 1)
struct expr_node * expr_node_dup_if_shared(struct expr_node * node) {
	if (node->rc == 1) {
		return node;
	}
	node->rc--;
	struct expr_node * new_node = mk_app_expr_node();
	expr_inc_rc(node->fn);
	expr_inc_rc(node->arg);
	new_node->fn = node->fn;
	new_node->arg = node->arg;
	new_node->bvar_range = node->bvar_range;
	return new_node;
}

typedef struct {
	char *str;
	size_t len;
} string_slice;

int string_slice_eq(string_slice a, string_slice b) {
	return a.len == b.len && memcmp(a.str, b.str, a.len) == 0;
}

struct parser {
	char * fname;
	char * str;
	char * end;
	char * line_start;
	int line_number;
	int has_error;
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

void parser_init(struct parser * parser, char * fname, char * str, size_t len) {
	parser->fname = fname;
	parser->str = str;
	parser->end = str + len;
	parser->line_start = str;
	parser->line_number = 1;
	parser->has_error = 0;
}

#define parser_error(parser, fmt, ...) \
  (void) (parser->has_error = 1, fprintf(stderr, "%s:%d:%d: error: " fmt "\n", (parser)->fname, (parser)->line_number, (int) ((parser)->str - (parser)->line_start), ##__VA_ARGS__))

int is_id_char(unsigned char ch) {
	return ch >= 0x21 && ch != '\\' && ch != '(' && ch != ')' && ch != '.' && ch != ',';
}

struct token next_token(struct parser * parser) {
	struct token token;
	while (1) {
		if (parser->str == parser->end) {
			token.type = TOKEN_EOF;
			return token;
		}
		char c = *parser->str;
		if (c == ' ' || c == '\t' || c == '\r') {
			parser->str++;
			continue;
		}
		if (c == '\n') {
			parser->str++;
			parser->line_number++;
			parser->line_start = parser->str;
			continue;
		}
		break;
	}
	token.value.str = parser->str;
	char c = *parser->str++;
	if (c == '\\') {
		token.type = TOKEN_LAMBDA;
		token.value.len = 1;
		return token;
	} else if (c == '(') {
		token.type = TOKEN_LPAREN;
		token.value.len = 1;
		return token;
	} else if (c == ')') {
		token.type = TOKEN_RPAREN;
		token.value.len = 1;
		return token;
	} else if (c == '.' || c == ',') {
		token.type = TOKEN_SEP;
		token.value.len = 1;
		return token;
	}
	if (!is_id_char(c)) {
		parser_error(parser, "Invalid character");
		token.type = TOKEN_ERROR;
		return token;
	}
	token.type = TOKEN_IDENT;
	while (parser->str != parser->end && is_id_char(*parser->str)) {
		parser->str++;
	}
	token.value.len = parser->str - token.value.str;
	return token;
}

struct lambda_parse_scope {
	struct expr * hole;
	unsigned int lam_depth;
	int var_index;
};

void update_expr_data(struct expr expr) {
	if (expr_node_is_app(expr.node)) {
		update_expr_data(expr.node->fn);
		update_expr_data(expr.node->arg);

		unsigned int fn_range = expr_get_bvar_range(expr.node->fn);
		unsigned int arg_range = expr_get_bvar_range(expr.node->arg);
		expr.node->bvar_range = fn_range < arg_range ? arg_range : fn_range;
	}
}

struct expr parse_expr(struct parser * parser) {
	struct expr result = { .node = NULL, .lam_count = 0 };
	struct lambda_parse_scope * scopes = malloc(sizeof(struct lambda_parse_scope) * 64);
	string_slice * vars = malloc(sizeof(string_slice) * 64);
	int scope_count = 1;
	int scope_capacity = 64;
	int var_count = 0;
	int var_capacity = 64;
	scopes[0].hole = &result;
	scopes[0].lam_depth = 0;
	scopes[0].var_index = 0;
	while (1) {
		struct token token = next_token(parser);
		if (token.type == TOKEN_EOF) {
			if (scopes[scope_count - 1].hole->node == NULL) {
				parser_error(parser, "Unexpected end of file, expected expression");
			} else if (scope_count > 1) {
				parser_error(parser, "Unclosed parenthesis");
			}
			break;
		} else if (token.type == TOKEN_ERROR) {
			continue; // We already should've reported an error, let's just try to continue
		} else if (token.type == TOKEN_RPAREN) {
			if (scope_count > 1) {
				scope_count--;
				if (scopes[scope_count].hole->node == NULL) {
					parser_error(parser, "Unexpected ')', expected expression");
					continue;
				}
				var_count = scopes[scope_count].var_index;
				continue;
			} else {
				parser_error(parser, "Unexpected ')', there is no corresponding '('");
				continue;
			}
		} else if (token.type == TOKEN_SEP) {
			parser_error(parser, "Unexpected token '%c'", token.value.str[0]);
			continue;
		}
		// Find a free hole
		struct expr * hole = scopes[scope_count - 1].hole;
		unsigned int hole_depth = scopes[scope_count - 1].lam_depth;
		if (hole->node != NULL) {
			struct expr_node * node = mk_app_expr_node();
			node->fn = (struct expr) { .node = hole->node, .lam_count = hole->lam_count - hole_depth };
			node->arg.node = NULL;
			*hole = (struct expr) { .node = node, .lam_count = hole_depth };
			hole = &node->arg;
			hole_depth = 0;
		}
		if (token.type == TOKEN_IDENT) {
			int i = var_count;
			while (i != 0) {
				i--;
				if (string_slice_eq(token.value, vars[i])) {
					*hole = (struct expr) { .node = mk_bvar_expr_node(var_count - i - 1), .lam_count = hole_depth };
					goto ident_end;
				}
			}
			char string_buf[1024];
			size_t len2 = token.value.len < 1023 ? token.value.len : 1023;
			memcpy(string_buf, token.value.str, len2);
			string_buf[len2] = 0;
			parser_error(parser, "Unknown identifier '%s'", string_buf);
ident_end:
			continue;
		}
		if (token.type == TOKEN_LPAREN) {
			if (scope_count >= scope_capacity) {
				scopes = realloc(vars, sizeof(struct lambda_parse_scope) * scope_capacity * 2);
				scope_capacity *= 2;
			}
			scopes[scope_count++] = (struct lambda_parse_scope) {
				.hole = hole,
				.lam_depth = hole_depth,
				.var_index = var_count
			};
			continue;
		}
		if (token.type == TOKEN_LAMBDA) {
			while (1) {
				token = next_token(parser);
				if (token.type == TOKEN_IDENT) {
					if (var_count >= var_capacity) {
						vars = realloc(vars, sizeof(string_slice) * var_capacity * 2);
						var_capacity *= 2;
					}
					vars[var_count++] = token.value;
					hole_depth++;
					continue;
				}
				if (token.type == TOKEN_SEP) {
					break;
				}
				if (token.type == TOKEN_ERROR) {
					break;
				}
				if (token.type == TOKEN_EOF) {
					parser_error(parser, "Unexpected end of file, expected identifier or '.'");
					break;
				}
				parser_error(parser, "Unexpected token '%c', expected identifier or '.'", token.value.str[0]);
				break;
			}
			scopes[scope_count - 1].hole = hole;
			scopes[scope_count - 1].lam_depth = hole_depth;
			continue;
		}
		parser_error(parser, "Unexpected token '%c'", token.value.str[0]);
	}
	update_expr_data(result);
	free(scopes);
	free(vars);
	return result;
}

// owned e, borrowed vals, owned return
struct expr expr_instantiate_rev_with_depth(struct expr e, unsigned int depth, struct expr * vals, unsigned int count) {
	depth += e.lam_count;
	if (expr_node_is_bvar(e.node)) {
		unsigned int var = expr_node_get_bvar(e.node);
		if (var < depth) {
			return e;
		}
		if (var - depth < count) {
			struct expr out = vals[var - depth];
			expr_inc_rc(out);
			return mk_multi_lam_expr(out, e.lam_count);
		}
		return (struct expr) { .node = mk_bvar_expr_node(var - 1), .lam_count = e.lam_count };
	} else if (expr_node_is_app(e.node)) {
		if (e.node->bvar_range <= depth) {
			return e;
		}
		struct expr_node * new_node = expr_node_dup_if_shared(e.node);
		new_node->fn = expr_instantiate_rev_with_depth(new_node->fn, depth, vals, count);
		new_node->arg = expr_instantiate_rev_with_depth(new_node->arg, depth, vals, count);

		unsigned int fn_range = expr_get_bvar_range(new_node->fn);
		unsigned int arg_range = expr_get_bvar_range(new_node->arg);
		new_node->bvar_range = fn_range < arg_range ? arg_range : fn_range;
		return (struct expr) { .node = new_node, .lam_count = e.lam_count };
	} else {
		return e;
	}
}

// owned fn, borrowed vals, owned return
// assumes forall i, i < count -> expr_get_bvar_range(vals[i]) == 0
struct expr expr_instantiate_rev(struct expr fn, struct expr * vals, unsigned int count) {
	// TODO: cache
	return expr_instantiate_rev_with_depth(fn, 0, vals, count);
}

unsigned int num_length(unsigned int val) {
	unsigned int len = 0;
	do {
		len++;
		val /= 10;
	} while (val > 0);
	return len;
}

void write_num(unsigned int val, char ** str) {
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

struct expr_buf {
	struct expr * data; // owned
	size_t capacity;
	size_t size;
	size_t off;
};

struct expr_buf expr_buf_new() {
	return (struct expr_buf) {
		.data = NULL,
		.capacity = 0,
		.size = 0,
		.off = 0
	};
}

void expr_buf_extend_front(struct expr_buf * buf, size_t n) {
	if (buf->off >= n) {
		buf->off -= n;
		return;
	}
	size_t diff = n - buf->off;
	if (buf->size + diff > buf->capacity) {
		if (buf->capacity < 16) buf->capacity = 16;
		while (buf->size + diff > buf->capacity) {
			buf->capacity *= 2;
		}
		buf->data = realloc(buf->data, sizeof(struct expr) * buf->capacity);
	}
	memmove(buf->data + n, buf->data + buf->off, sizeof(struct expr) * (buf->size - buf->off));
	buf->off = 0;
	buf->size += diff;
}

// owned e
void expr_buf_push(struct expr_buf * buf, struct expr e) {
	if (buf->size >= buf->capacity) {
		if (buf->off > 0) {
			buf->size -= buf->off;
			memmove(buf->data, buf->data + buf->off, sizeof(struct expr) * buf->size);
		} else {
			size_t new_capacity = buf->capacity * 2;
			if (new_capacity < 16) new_capacity = 16;
			buf->data = realloc(buf->data, sizeof(struct expr) * new_capacity);
			buf->capacity = new_capacity;
		}
	}
	buf->data[buf->size++] = e;
}

char * debug_expr_to_str(struct expr expr) {
	size_t sz = 0;
	dbg_print_expr_size(expr, &sz, 0);
	char * buffer = malloc(sz + 1);
	char * pos = buffer;
	dbg_print_expr(expr, &pos, 0);
	*pos = 0;
	return buffer;
}

struct expr expr_bool_false() {
	return mk_multi_lam_expr(mk_bvar_expr(0), 2);
}

struct expr expr_bool_true() {
	return mk_multi_lam_expr(mk_bvar_expr(1), 2);
}

// owned e
void eval_by_reduce(struct expr e, int trace) {
	struct expr_buf args = expr_buf_new();
	int op_stream = 0;
	unsigned char byte_read = 0;
	unsigned char byte_read_mask = 0;
	unsigned char byte_write = 0;
	unsigned char byte_write_mask = 0x80;
start:
	if (e.lam_count > 0) {
		// beta-reduce
		struct expr body = { .node = e.node, .lam_count = 0 };
		size_t lam_count = e.lam_count;
		if (args.size - args.off < lam_count) {
			size_t diff = lam_count - args.size;
			expr_buf_extend_front(&args, diff);
			if (op_stream < 0) {
				for (size_t i = 0; i < diff; i++) {
					args.data[args.off + i] = mk_op_expr(0);
				}
			} else {
				for (size_t i = 0; i < diff; i++) {
					args.data[args.off + i] = mk_op_expr((int) (op_stream + diff - i - 1));
				}
				op_stream = (int) (op_stream + diff);
			}
		}
		args.size -= lam_count;
		e = expr_instantiate_rev(body, args.data + args.size, (unsigned int) lam_count);
		for (size_t i = 0; i < lam_count; i++) {
			expr_dec_rc(args.data[args.size + i]);
		}
		goto start;
	}
	if (expr_node_is_app(e.node)) {
		struct expr fn = e.node->fn;
		struct expr arg = e.node->arg;
		expr_inc_rc(fn);
		expr_inc_rc(arg);
		expr_dec_rc(e);
		expr_buf_push(&args, arg);
		e = fn;
		goto start;
	}
	if (expr_node_is_op(e.node)) {
		int op = expr_node_get_op(e.node);
		for (size_t i = args.off; i + 1 < args.size; i++) {
			expr_dec_rc(args.data[i]);
		}
		e = args.size > args.off ? args.data[args.size - 1] : mk_op_expr(0);
		if (trace) {
			// debug print
			char * str = debug_expr_to_str(e);
			printf("operation %d with %s\n", op, str);
			free(str);
			/*for (size_t i = args.size; i > args.off;) {
				i--;
				str = debug_expr_to_str(args.data[i]);
					printf("arg (%lld): %s\n", i, str);
				free(str);
			}*/
		}
		args.size = 0;
		args.off = 0;
		switch (op) {
		case 1: {
			if (byte_read_mask == 0) {
				byte_read = 0;
				fread(&byte_read, 1, 1, stdin);
				byte_read_mask = 0x80;
			}
			if (byte_read & byte_read_mask) {
				expr_buf_push(&args, expr_bool_true());
			} else {
				expr_buf_push(&args, expr_bool_false());
			}
			byte_read_mask >>= 1;
		} break;
		case 2: {
			byte_write_mask >>= 1;
			if (byte_write_mask == 0) {
				fwrite(&byte_write, 1, 1, stdout);
				byte_write = 0;
				byte_write_mask = 0x80;
			}
		} break;
		case 3: {
			byte_write |= byte_write_mask;
			byte_write_mask >>= 1;
			if (byte_write_mask == 0) {
				fwrite(&byte_write, 1, 1, stdout);
				byte_write = 0;
				byte_write_mask = 0x80;
			}
		} break;
		default: goto end;
		}
		op_stream = -1;
		goto start;
	}
end:
	expr_dec_rc(e);
	for (size_t i = args.off; i < args.size; i++) {
		expr_dec_rc(args.data[i]);
	}
	free(args.data);
	return;
}

#define USAGE \
	"Usage: %s [options] <file>\n" \
	"\n" \
	"Options:\n" \
	"--trace: Show reduction trace\n" \

void show_usage(char **argv) {
	fprintf(stderr, USAGE, argv[0]);
}

int main(int argc, char **argv) {
	int enable_trace = 0;
	int argi = 1;
	while (argi < argc) {
		char * arg = argv[argi];
		if (arg[0] == '-' && arg[1] == '-') {
			argi++;
			if (arg[2] == 0) {
				break;
			}
			if (strcmp(arg, "--trace") == 0) {
				enable_trace = 1;
			} else {
				fprintf(stderr, "Unknown option %s", arg);
				show_usage(argv);
				return 1;
			}
		} else {
			break;
		}
	}
	if (argi >= argc) {
		show_usage(argv);
		return 1;
	}
	FILE *f = fopen(argv[argi], "rb");
	if (f == NULL) {
		fprintf(stderr, "Failed to open file %s\n", argv[argi]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	size_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *data = malloc(fsize);
	fread(data, fsize, 1, f);
	fclose(f);

	struct parser parser;
	parser_init(&parser, argv[argi], data, fsize);
	struct expr expr = parse_expr(&parser);
	free(data);
	if (parser.has_error) {
		expr_dec_rc(expr);
		return 1;
	}

	printf("Parsing success!\n");

	eval_by_reduce(expr, enable_trace);
}
