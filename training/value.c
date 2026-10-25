#include "value.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The tape: every temporary Value is appended here when it is created.
 * Because a Value can only be created after its inputs exist, the tape order
 * IS a valid topological order of the graph. So backward() just walks the
 * tape in reverse; no recursion or visited-flags needed (micrograd builds
 * the same ordering with a DFS).
 */
static Value **tape = NULL;
static size_t tape_len = 0, tape_cap = 0;

static Value *alloc_value(double data, Op op, Value *a, Value *b, int track) {
    Value *v = calloc(1, sizeof *v);
    if (!v) { fprintf(stderr, "out of memory\n"); exit(1); }
    v->data = data;
    v->op = op;
    v->prev[0] = a;
    v->prev[1] = b;
    if (track) {
        if (tape_len == tape_cap) {
            tape_cap = tape_cap ? tape_cap * 2 : 1024;
            tape = realloc(tape, tape_cap * sizeof *tape);
            if (!tape) { fprintf(stderr, "out of memory\n"); exit(1); }
        }
        tape[tape_len++] = v;
    }
    return v;
}

Value *val_param(double data, const char *label) {
    Value *v = alloc_value(data, OP_LEAF, NULL, NULL, 0);
    if (label) snprintf(v->label, sizeof v->label, "%s", label);
    return v;
}

void val_free(Value *v) { free(v); }

Value *val_new(double data)          { return alloc_value(data, OP_LEAF, NULL, NULL, 1); }
Value *val_add(Value *a, Value *b)   { return alloc_value(a->data + b->data, OP_ADD, a, b, 1); }
Value *val_sub(Value *a, Value *b)   { return alloc_value(a->data - b->data, OP_SUB, a, b, 1); }
Value *val_mul(Value *a, Value *b)   { return alloc_value(a->data * b->data, OP_MUL, a, b, 1); }
Value *val_tanh(Value *a)            { return alloc_value(tanh(a->data), OP_TANH, a, NULL, 1); }

/* Local derivative * incoming gradient (out->grad), pushed into the inputs. */
static void backprop_one(Value *out) {
    Value *a = out->prev[0], *b = out->prev[1];
    switch (out->op) {
    case OP_ADD:   /* sum node passes the gradient through             */
        a->grad += out->grad;
        b->grad += out->grad;
        break;
    case OP_SUB:
        a->grad += out->grad;
        b->grad -= out->grad;
        break;
    case OP_MUL:   /* multiply node: each input gets the OTHER input   */
        a->grad += b->data * out->grad;
        b->grad += a->data * out->grad;
        break;
    case OP_TANH:  /* d tanh(z)/dz = 1 - tanh(z)^2 = 1 - out^2         */
        a->grad += (1.0 - out->data * out->data) * out->grad;
        break;
    case OP_LEAF:
        break;
    }
}

void val_backward(Value *root) {
    root->grad = 1.0;                       /* dL/dL = 1 */
    for (size_t i = tape_len; i-- > 0; )
        backprop_one(tape[i]);
}

void val_tape_clear(void) {
    for (size_t i = 0; i < tape_len; i++) free(tape[i]);
    tape_len = 0;
}
