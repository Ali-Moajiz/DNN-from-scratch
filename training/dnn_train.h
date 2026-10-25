#ifndef DNN_TRAIN_H
#define DNN_TRAIN_H
/*
 * dnn_train.h : a small fully-connected network (Neuron -> Layer -> MLP,
 * like micrograd's nn module) built entirely from Value pointers, plus the
 * training loop.
 */
#include "value.h"
#include "train_data.h"

typedef struct {
    Value **w;     /* nin weights, each a Value*  */
    Value  *b;     /* bias                        */
    int     nin;
} Neuron;

typedef struct {
    Neuron *neurons;
    int nin, nout;
} Layer;

typedef struct {
    Layer  *layers;
    int     n_layers;
    int     n_in, n_out;
    Value **params;     /* every weight and bias, flattened (for zero_grad / update) */
    int     n_params;
} MLP;

typedef struct {
    int      epochs;
    double   lr;
    int      print_every;   /* 0 = silent */
} TrainConfig;

/* sizes = {2, 2, 1} means 2 inputs -> 2 hidden neurons -> 1 output.
 * Every neuron uses tanh. Weights start uniform in [-1, 1], biases at 0. */
MLP  *mlp_create(const int *sizes, int n_sizes, unsigned seed);
void  mlp_free(MLP *m);
void  mlp_print_params(const MLP *m);

/* Build the forward graph. x has m->n_in Values, out receives m->n_out Values. */
void  mlp_forward(const MLP *m, Value **x, Value **out);

/* Convenience inference on plain doubles (cleans up its own graph). */
void  dnn_predict(const MLP *m, const double *x, double *out);

/* Full-batch gradient descent on mean squared error.
 * Returns the final loss, or -1 on a size mismatch. */
double dnn_train(MLP *m, const Dataset *ds, const TrainConfig *cfg);

/* Compares backprop gradients with numerical (nudge-and-measure) gradients.
 * Prints each parameter and returns the largest absolute difference. */
double dnn_grad_check(MLP *m, const Dataset *ds);

#endif
