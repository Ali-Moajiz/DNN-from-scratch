#include "train_data.h"
#include <stdio.h>

/*
 * XOR: the classic "needs a hidden layer" problem.
 * No single straight line separates the 1s from the 0s, so a network with no
 * hidden layer can never learn it, but 2 tanh hidden neurons can.
 * Targets are -1 / +1 (not 0 / 1) because the output neuron is tanh, whose
 * range is (-1, 1).
 */
static const double xor_X[4 * 2] = {
    /* x1  x2 */
       0,  0,
       0,  1,
       1,  0,
       1,  1,
};

static const double xor_Y[4 * 1] = {
    /* y */
      -1,
       1,
       1,
      -1,
};

const Dataset xor_dataset = {
    .n_samples  = 4,
    .n_features = 2,
    .n_targets  = 1,
    .X = xor_X,
    .Y = xor_Y,
};

/*
 * Sine wave: one period, y = sin(x) for x from -pi to +pi.
 * The tables are fixed values (rounded to 4 decimals), written out here as
 * plain arrays. Targets lie in [-1, 1], exactly the range of a tanh output.
 *
 * TRAIN: 32 evenly spaced points.
 * TEST : 8 points that fall BETWEEN the training points. They are never
 *        used for training; they check that the net learned the curve and
 *        not just the samples.
 */
static const double sine_train_X[32 * 1] = {
    /* x */
    -3.1416, -2.9389, -2.7362, -2.5335,
    -2.3309, -2.1282, -1.9255, -1.7228,
    -1.5201, -1.3174, -1.1148, -0.9121,
    -0.7094, -0.5067, -0.3040, -0.1013,
     0.1013,  0.3040,  0.5067,  0.7094,
     0.9121,  1.1148,  1.3174,  1.5201,
     1.7228,  1.9255,  2.1282,  2.3309,
     2.5335,  2.7362,  2.9389,  3.1416,
};

static const double sine_train_Y[32 * 1] = {
    /* sin(x) */
     0.0000, -0.2013, -0.3944, -0.5713,
    -0.7248, -0.8486, -0.9377, -0.9885,
    -0.9987, -0.9681, -0.8978, -0.7908,
    -0.6514, -0.4853, -0.2993, -0.1011,
     0.1011,  0.2993,  0.4853,  0.6514,
     0.7908,  0.8978,  0.9681,  0.9987,
     0.9885,  0.9377,  0.8486,  0.7248,
     0.5713,  0.3944,  0.2013, -0.0000,
};

const Dataset sine_dataset = {
    .n_samples  = 32,
    .n_features = 1,
    .n_targets  = 1,
    .X = sine_train_X,
    .Y = sine_train_Y,
};

static const double sine_test_X[8 * 1] = {
    /* x */
    -3.0403, -2.2295, -1.4188, -0.6081,
     0.2027,  1.0134,  1.8242,  2.6349,
};

static const double sine_test_Y[8 * 1] = {
    /* sin(x) */
    -0.1011, -0.7908, -0.9885, -0.5713,
     0.2013,  0.8486,  0.9681,  0.4853,
};

const Dataset sine_test_dataset = {
    .n_samples  = 8,
    .n_features = 1,
    .n_targets  = 1,
    .X = sine_test_X,
    .Y = sine_test_Y,
};

void dataset_print(const Dataset *ds) {
    printf("Dataset: %zu samples, %zu features, %zu targets\n",
           ds->n_samples, ds->n_features, ds->n_targets);
    for (size_t i = 0; i < ds->n_samples; i++) {
        printf("  [%zu] x = (", i);
        for (size_t j = 0; j < ds->n_features; j++)
            printf("%s%g", j ? ", " : "", dataset_x(ds, i)[j]);
        printf(")  y = (");
        for (size_t k = 0; k < ds->n_targets; k++)
            printf("%s%g", k ? ", " : "", dataset_y(ds, i)[k]);
        printf(")\n");
    }
}