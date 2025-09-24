#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expr.h"
#include "parser.h"
#include "beta_eval.h"

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
	if (parser.error_count) {
		fprintf(stderr, "%d errors\n", parser.error_count);
		expr_dec_rc(expr);
		return 1;
	}

	eval_by_reduce(expr, enable_trace);
}
