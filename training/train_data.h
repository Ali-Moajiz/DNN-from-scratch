#ifndef TRAIN_DATA_H
#define TRAIN_DATA_H
/*
 * train_data.h : the training data is just arrays.
 *
 * A Dataset does not own or create anything. It is a small descriptor that
 * POINTS at two row-major tables of doubles (one row per sample):
 *
 *     X : n_samples rows x n_features columns   (inputs)
 *     Y : n_samples rows x n_targets  columns   (expected outputs)
 *
 * To add data, write the arrays in train_data.c and one Dataset line that
 * points at them. The training code only reads the three sizes and the two
 * pointers, so it works with any data. Later, a file loader only has to fill
 * these same two pointers.
 */
#include <stddef.h>

typedef struct Dataset {
    size_t n_samples;
    size_t n_features;
    size_t n_targets;
    const double *X;   /* n_samples * n_features, row-major */
    const double *Y;   /* n_samples * n_targets,  row-major */
} Dataset;

/* The data sets (defined in train_data.c) */
extern const Dataset xor_dataset;
extern const Dataset sine_dataset;        /* 32 training points of y = sin(x)   */
extern const Dataset sine_test_dataset;   /* 8 held-out points between them     */

/* Pointer to the start of sample i's inputs / targets. */
static inline const double *dataset_x(const Dataset *ds, size_t i) { return ds->X + i * ds->n_features; }
static inline const double *dataset_y(const Dataset *ds, size_t i) { return ds->Y + i * ds->n_targets; }

void dataset_print(const Dataset *ds);

#endif