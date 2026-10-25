#ifndef VALUE_H
#define VALUE_H
/*
 * value.h : a tiny autograd engine (the C version of micrograd's Value class).
 *
 * Every number in the network (weights, biases, inputs, intermediate results,
 * the loss) is a pointer to a Value. Each Value remembers:
 *   - its data and its gradient dL/d(this value)
 *   - which operation created it (op) and its inputs (prev[0], prev[1])
 * That is enough to run backprop: the chain rule is applied one op at a time.
 */
#include <stddef.h>

typedef enum { OP_LEAF, OP_ADD, OP_SUB, OP_MUL, OP_TANH } Op;

typedef struct Value {
    double data;             /* forward value                       */
    double grad;             /* dL/d(this), filled in by backward   */
    struct Value *prev[2];   /* inputs of the op that produced this */
    Op op;                   /* the op that produced this           */
    char label[40];          /* optional name, for printing         */
} Value;

/* Persistent value (weights, biases). NOT on the tape: you free it yourself. */
Value *val_param(double data, const char *label);
void   val_free(Value *v);

/* Temporary value (inputs, intermediates, loss). Recorded on the tape and
 * released all at once by val_tape_clear(). */
Value *val_new(double data);

/* Operations. Each one creates a new temporary Value. */
Value *val_add(Value *a, Value *b);
Value *val_sub(Value *a, Value *b);
Value *val_mul(Value *a, Value *b);
Value *val_tanh(Value *a);

/* Sets root->grad = 1 and propagates gradients back to every Value that
 * contributed to root. Gradients ACCUMULATE (+=), so zero params first. */
void val_backward(Value *root);

/* Free every temporary Value created since the last clear. Call once per
 * training step, after the parameter update. */
void val_tape_clear(void);

#endif
