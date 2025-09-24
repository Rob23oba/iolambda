#include <string.h>
#include "parser.h"

int string_slice_eq(string_slice a, string_slice b) {
	return a.len == b.len && memcmp(a.str, b.str, a.len) == 0;
}

void parser_init(struct parser * parser, char * fname, char * str, size_t len) {
	parser->fname = fname;
	parser->str = str;
	parser->end = str + len;
	parser->line_start = str;
	parser->line_number = 1;
	parser->error_count = 0;
}

#define parser_error(parser, fmt, ...) \
	(void) (parser->error_count++ < 100 ? fprintf(stderr, "%s:%d:%d: error: " fmt "\n", (parser)->fname, (parser)->line_number, (int) ((parser)->str - (parser)->line_start), ##__VA_ARGS__) : (void) 0)

static int is_id_char(unsigned char ch) {
	return ch >= 0x21 && ch != '\\' && ch != '(' && ch != ')' && ch != '.' && ch != ',';
}

static struct token parser_next_token(struct parser * parser) {
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
		struct token token = parser_next_token(parser);
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
				token = parser_next_token(parser);
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
