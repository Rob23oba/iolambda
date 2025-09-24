#include <stdio.h>
#include "print_expr.h"
#include "expr_buf.h"
#include "beta_eval.h"

static struct expr expr_bool_false() {
	return mk_multi_lam_expr(mk_bvar_expr(0), 2);
}

static struct expr expr_bool_true() {
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
			char * str = dbg_expr_to_str(e);
			printf("operation %d with %s\n", op, str);
			free(str);
			/*for (size_t i = args.size; i > args.off;) {
				i--;
				str = dbg_expr_to_str(args.data[i]);
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
