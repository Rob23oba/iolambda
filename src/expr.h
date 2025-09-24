#ifndef EXPR_H
#define EXPR_H

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

static inline int expr_node_is_bvar(struct expr_node * node) {
	return (((size_t) node) & 3) == 1;
}

static inline int expr_node_is_op(struct expr_node * node) {
	return (((size_t) node) & 3) == 3;
}

static inline int expr_node_is_app(struct expr_node * node) {
	return node != NULL && (((size_t) node) & 1) == 0;
}

static inline unsigned int expr_node_get_bvar(struct expr_node * node) {
	return (unsigned int) (((size_t) node) >> 2);
}

static inline unsigned int expr_node_get_op(struct expr_node * node) {
	return (unsigned int) (((size_t) node) >> 2);
}

unsigned int expr_get_bvar_range(struct expr e);

static inline struct expr_node * mk_bvar_expr_node(unsigned int var_index) {
	return (struct expr_node *) ((((size_t) var_index << 2) | 1));
}

static inline struct expr_node * mk_op_expr_node(unsigned int op_index) {
	return (struct expr_node *) ((((size_t) op_index << 2) | 3));
}

static inline struct expr mk_bvar_expr(unsigned int var_index) {
	return (struct expr) { .node = mk_bvar_expr_node(var_index), .lam_count = 0 };
}

static inline struct expr mk_op_expr(unsigned int op_index) {
	return (struct expr) { .node = mk_op_expr_node(op_index), .lam_count = 0 };
}

struct expr_node * mk_app_expr_node();

// owned fn, owned arg, owned return
struct expr mk_app_expr(struct expr fn, struct expr arg);

// owned body, owned return
static inline struct expr mk_lam_expr(struct expr body) {
	return (struct expr) { .node = body.node, .lam_count = body.lam_count + 1 };
}

// owned body, owned return
static inline struct expr mk_multi_lam_expr(struct expr body, unsigned int n) {
	return (struct expr) { .node = body.node, .lam_count = body.lam_count + n };
}

void expr_inc_rc(struct expr e);
void expr_dec_rc(struct expr e);

// owned e, owned return (with rc 1)
struct expr_node * expr_node_dup_if_shared(struct expr_node * node);

void update_expr_data(struct expr expr);

// owned e, borrowed vals, owned return
struct expr expr_instantiate_rev(struct expr e, struct expr * vals, unsigned int count);

#endif
