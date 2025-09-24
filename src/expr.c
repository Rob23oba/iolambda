#include <stdlib.h>
#include "expr.h"

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

void update_expr_data(struct expr expr) {
	if (expr_node_is_app(expr.node)) {
		update_expr_data(expr.node->fn);
		update_expr_data(expr.node->arg);

		unsigned int fn_range = expr_get_bvar_range(expr.node->fn);
		unsigned int arg_range = expr_get_bvar_range(expr.node->arg);
		expr.node->bvar_range = fn_range < arg_range ? arg_range : fn_range;
	}
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
