Lambda calculus with IO
=======================

Experiment for extending untyped lambda calculus with I/O using continuation passing.
Programs are written in regular lambda calculus, they can however take in I/O operations as arguments.
I/O operations are numbered and represented through an additional kind of expressions, operation expressions.
Operation expressions take in a single continuation that will be run with additional arguments resulting from the operation.
You can't write these down in programs directly but they will be provided as arguments for the expression (I'll refer to them as `!n` here where `n` is the operation number).

Syntax
------

Lambda expressions are written as `\a b c. x`. Applications are written by putting two terms next to each other. Parentheses are also there. You can use a nullary lambda `\.` as a piping operator.

Semantics
---------

The semantics are defined using an evaluation function `eval` that takes in an expression and another infinite stream of expressions.
Rules:
1. Evaluating a lambda: `eval(\x. b, arg, args...) = eval(b[x/arg], args...)` (beta reduction)
2. Evaluating an application: `eval(f a, args...) = eval(f, a, args...)`
3. Evaluating a free variable: `eval(x, args...) = <do nothing and halt>`
4. Evaluating an operation expression: `eval(!n, arg, args...) = let new_args := <run operation n> in eval(arg, new_args, !0, !0, !0, ...)`

Operation table:

Number | Name | Description                                  | New arguments
----------------------------------------------------------------------------
0      | halt | Stops program execution                      | None
1      | read | Reads a single bit from stdin (big-endian)   | for bit 0: `\x y. y`, for bit 1: `\x y. x`
2      | bit0 | Writes a 0 bit (big-endian)                  | None
3      | bit1 | Writes a 1 bit (big-endian)                  | None

Currently, unknown operations behave like `halt`.
