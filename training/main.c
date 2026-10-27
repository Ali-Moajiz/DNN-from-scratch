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
        if (verbose) {
            printf("  x = [");
            for (size_t j = 0; j < ds->n_features; j++)
                printf("%s% .4f", j ? ", " : "", dataset_x(ds, i)[j]);
            printf("]   predicted % .4f   target % .4f   error % .4f\n",
                   out, dataset_y(ds, i)[0], err);
        }
    }
    return sum / (double)ds->n_samples;
}

int main(void) {
    const Dataset *train = &xor_dataset;

    const int sizes[] = {2, 2, 1};          /* 2 inputs -> 2 hidden neurons -> 1 output */
    MLP *net = mlp_create(sizes, 3, 42);

    printf("Gradient check (backprop vs nudge-and-measure):\n");
    printf("  largest difference: %.2e\n", dnn_grad_check(net, train));

    printf("\nTraining:\n");
    TrainConfig cfg = { .epochs = 200000, .lr = 0.3, .print_every = 2000 };
    dnn_train(net, train, &cfg);

    printf("\nTrained weights and biases:\n");
    mlp_print_params(net);

    printf("\nTrain MSE: %.6f\n", evaluate(net, train, 0));
    printf("\nXOR predictions:\n");
    evaluate(net, train, 1);

    mlp_free(net);
    return 0;
}