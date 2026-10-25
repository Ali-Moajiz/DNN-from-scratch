#include "dnn_train.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- construction ---------- */

static double rand_uniform(void) { return 2.0 * ((double)rand() / RAND_MAX) - 1.0; }

static Neuron neuron_create(int nin, int layer, int idx)
{
    Neuron n;
    char lab[40];
    n.nin = nin;
    n.w = malloc((size_t)nin * sizeof *n.w);
    for (int i = 0; i < nin; i++)
    {
        snprintf(lab, sizeof lab, "L%dN%d.w%d", layer, idx, i);
        n.w[i] = val_param(rand_uniform(), lab);
    }
    snprintf(lab, sizeof lab, "L%dN%d.b", layer, idx);
    n.b = val_param(0.0, lab);
    return n;
}

MLP *mlp_create(const int *sizes, int n_sizes, unsigned seed)
{
    srand(seed);
    MLP *m = calloc(1, sizeof *m);
    m->n_layers = n_sizes - 1;
    m->n_in = sizes[0];
    m->n_out = sizes[n_sizes - 1];
    m->layers = calloc((size_t)m->n_layers, sizeof *m->layers);

    for (int l = 0; l < m->n_layers; l++)
    {
        Layer *L = &m->layers[l];
        L->nin = sizes[l];
        L->nout = sizes[l + 1];
        L->neurons = malloc((size_t)L->nout * sizeof *L->neurons);
        for (int j = 0; j < L->nout; j++)
        {
            L->neurons[j] = neuron_create(L->nin, l, j);
            m->n_params += L->nin + 1;
        }
    }
    /* flatten all parameters into one array */
    m->params = malloc((size_t)m->n_params * sizeof *m->params);
    int k = 0;
    for (int l = 0; l < m->n_layers; l++)
        for (int j = 0; j < m->layers[l].nout; j++)
        {
            Neuron *n = &m->layers[l].neurons[j];
            for (int i = 0; i < n->nin; i++)
                m->params[k++] = n->w[i];
            m->params[k++] = n->b;
        }
    return m;
}

void mlp_free(MLP *m)
{
    if (!m)
        return;
    for (int l = 0; l < m->n_layers; l++)
    {
        for (int j = 0; j < m->layers[l].nout; j++)
        {
            Neuron *n = &m->layers[l].neurons[j];
            for (int i = 0; i < n->nin; i++)
                val_free(n->w[i]);
            val_free(n->b);
            free(n->w);
        }
        free(m->layers[l].neurons);
    }
    free(m->layers);
    free(m->params);
    free(m);
}

void mlp_print_params(const MLP *m)
{
    for (int i = 0; i < m->n_params; i++)
        printf("  %-10s = % .4f\n", m->params[i]->label, m->params[i]->data);
}

/* ---------- forward ---------- */

/* z = w0*x0 + w1*x1 + ... + b ;  out = tanh(z)   (same as your diagram) */
static Value *neuron_forward(const Neuron *n, Value **x)
{
    Value *z = n->b;
    for (int i = 0; i < n->nin; i++)
        z = val_add(z, val_mul(n->w[i], x[i]));
    return val_tanh(z);
}

void mlp_forward(const MLP *m, Value **x, Value **out)
{
    Value **cur = x;
    for (int l = 0; l < m->n_layers; l++)
    {
        const Layer *L = &m->layers[l];
        Value **next = malloc((size_t)L->nout * sizeof *next);
        for (int j = 0; j < L->nout; j++)
            next[j] = neuron_forward(&L->neurons[j], cur);
        if (cur != x)
            free(cur);
        cur = next;
    }
    memcpy(out, cur, (size_t)m->n_out * sizeof *out);
    free(cur);
}

void dnn_predict(const MLP *m, const double *x, double *out)
{
    Value **in = malloc((size_t)m->n_in * sizeof *in);
    Value **o = malloc((size_t)m->n_out * sizeof *o);
    for (int i = 0; i < m->n_in; i++)
        in[i] = val_new(x[i]);
    mlp_forward(m, in, o);
    for (int k = 0; k < m->n_out; k++)
        out[k] = o[k]->data;
    free(in);
    free(o);
    val_tape_clear();
}

/* ---------- loss ---------- */

/* Mean squared error over the whole dataset, as one Value (a graph). */
static Value *compute_loss(const MLP *m, const Dataset *ds)
{
    Value **in = malloc(ds->n_features * sizeof *in);
    Value **out = malloc(ds->n_targets * sizeof *out);
    Value *loss = val_new(0.0);
    for (size_t s = 0; s < ds->n_samples; s++)
    {
        const double *x = dataset_x(ds, s), *y = dataset_y(ds, s);
        for (size_t j = 0; j < ds->n_features; j++)
            in[j] = val_new(x[j]);
        mlp_forward(m, in, out);
        for (size_t k = 0; k < ds->n_targets; k++)
        {
            Value *diff = val_sub(out[k], val_new(y[k])); /* o - y     */
            loss = val_add(loss, val_mul(diff, diff));    /* + (o-y)^2 */
        }
    }
    free(in);
    free(out);
    return val_mul(loss, val_new(1.0 / (double)ds->n_samples));
}

static void zero_grad(MLP *m)
{
    for (int i = 0; i < m->n_params; i++)
        m->params[i]->grad = 0.0;
}

static int shapes_ok(const MLP *m, const Dataset *ds)
{
    if ((int)ds->n_features != m->n_in || (int)ds->n_targets != m->n_out)
    {
        fprintf(stderr, "shape mismatch: net is %d->%d, data is %zu->%zu\n",
                m->n_in, m->n_out, ds->n_features, ds->n_targets);
        return 0;
    }
    return 1;
}

/* ---------- training ---------- */

double dnn_train(MLP *m, const Dataset *ds, const TrainConfig *cfg)
{
    if (!shapes_ok(m, ds))
        return -1.0;
    double last = 0.0;
    int counter = 0;

    for (int epoch = 0; epoch <= cfg->epochs; epoch++)
    {
        Value *loss = compute_loss(m, ds); /* 1. forward  */

        if (last == loss->data)
        {
          counter++;
          printf("watching counter value: %d\n", counter);
        }

        last = loss->data;

        if (cfg->print_every && epoch % cfg->print_every == 0)
            printf("epoch %5d   loss %.6f\n", epoch, last);
        

        if (epoch == cfg->epochs || counter == 5)
        {
            printf("last epoch %5d   loss %.6f\n", epoch, last);
            val_tape_clear();
            break;
        }

        zero_grad(m); /* 2. backward */
        val_backward(loss);

        for (int i = 0; i < m->n_params; i++) /* 3. update   */
            m->params[i]->data -= cfg->lr * m->params[i]->grad;

        val_tape_clear();
    }
    return last;
}

/* ---------- gradient check ---------- */

double dnn_grad_check(MLP *m, const Dataset *ds)
{
    if (!shapes_ok(m, ds))
        return -1.0;
    const double eps = 1e-6;
    double *analytic = malloc((size_t)m->n_params * sizeof *analytic);
    double max_diff = 0.0;

    zero_grad(m);
    Value *loss = compute_loss(m, ds);
    val_backward(loss);
    for (int i = 0; i < m->n_params; i++)
        analytic[i] = m->params[i]->grad;
    val_tape_clear();

    printf("  %-10s %12s %12s\n", "param", "backprop", "numerical");
    for (int i = 0; i < m->n_params; i++)
    {
        Value *p = m->params[i];
        double orig = p->data;
        p->data = orig + eps;
        double up = compute_loss(m, ds)->data;
        val_tape_clear();
        p->data = orig - eps;
        double dn = compute_loss(m, ds)->data;
        val_tape_clear();
        p->data = orig;
        double numeric = (up - dn) / (2.0 * eps);
        double diff = fabs(numeric - analytic[i]);
        if (diff > max_diff)
            max_diff = diff;
        printf("  %-10s % 12.6f % 12.6f\n", p->label, analytic[i], numeric);
    }
    free(analytic);
    zero_grad(m);
    return max_diff;
}
