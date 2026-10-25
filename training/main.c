#include <stdio.h>
#include "dnn_train.h"
#include "train_data.h"

/* Mean squared error of the net on a dataset (no graph is kept). */
static double evaluate(const MLP *net, const Dataset *ds, int verbose) {
    double sum = 0.0;
    for (size_t i = 0; i < ds->n_samples; i++) {
        double out;
        dnn_predict(net, dataset_x(ds, i), &out);
        double err = out - dataset_y(ds, i)[0];
        sum += err * err;
        if (verbose)
            printf("  x = % .4f   predicted % .4f   target % .4f   error % .4f\n",
                   dataset_x(ds, i)[0], out, dataset_y(ds, i)[0], err);
    }
    return sum / (double)ds->n_samples;
}

int main(void) {
    const Dataset *train = &sine_dataset;        /* later: a dataset parsed from a file */
    const Dataset *test  = &sine_test_dataset;

    const int sizes[] = {1, 6, 1};          /* 1 input -> hidden layer -> 1 output */
    MLP *net = mlp_create(sizes, 3, 42);

    printf("Gradient check (backprop vs nudge-and-measure):\n");
    printf("  largest difference: %.2e\n", dnn_grad_check(net, train));

    printf("\nTraining:\n");
    TrainConfig cfg = { .epochs = 200000, .lr = 0.3, .print_every = 2000 };
    dnn_train(net, train, &cfg);

    printf("\nTrain MSE: %.6f\n", evaluate(net, train, 0));
    printf("\nHeld-out points (never used in training):\n");
    printf("Test MSE:  %.6f\n", evaluate(net, test, 1));

    mlp_free(net);
    return 0;
}