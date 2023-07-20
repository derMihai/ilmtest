/* This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <https://unlicense.org>
 * */
#include <pthread.h>
#include <stdlib.h>
#include <gsl/gsl_blas.h>
#include <math.h>
#include <time.h>
#include "mat_cbuf.h"

#define CBUF_SIZE 16
#define MAT_HW 1024
#define SUB_MAT_CNT 4
#define WORKERS 4

SemCbuf_MatMul *cbuf = NULL;

static void *worker_f(void *arg)
{
    (void)arg;

    MatMul work;
    while (semcbuf_pop_MatMul(cbuf, &work), work.a != NULL) {
        gsl_matrix *result = gsl_matrix_alloc(work.b->size1, work.a->size2);

        gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, work.a, work.b, 0.0, result);

        *work.result = result;
    }

    return NULL;
}

void split_mat(gsl_matrix *m, gsl_matrix_view split[2][2])
{
    split[0][0] = gsl_matrix_submatrix(m, 
                                    0,
                                    0,
                                    m->size1 / 2, 
                                    m->size2 / 2);
    split[0][1] = gsl_matrix_submatrix(m, 
                                    0,
                                    m->size1 / 2, 
                                    m->size1 - m->size1 / 2, 
                                    m->size2 / 2);
    split[1][0] = gsl_matrix_submatrix(m, 
                                    m->size2 / 2,
                                    0, 
                                    m->size1 / 2, 
                                    m->size2 - m->size2 / 2);
    split[1][1] = gsl_matrix_submatrix(m, 
                                    m->size1 / 2, 
                                    m->size2 / 2,
                                    m->size1 - m->size1 / 2, 
                                    m->size2 - m->size2 / 2);
}

int main() 
{    
    int res;
    pthread_t workers[WORKERS];

    cbuf = malloc(sizeof(*cbuf));
    assert(cbuf);

    MatMul *cbuf_buf = malloc(sizeof(MatMul) * CBUF_SIZE);
    assert(cbuf_buf);

    semcbuf_init_MatMul(cbuf, cbuf_buf, CBUF_SIZE);

    float elem = 1.0;

    gsl_matrix *mat_a = gsl_matrix_alloc(MAT_HW, MAT_HW);
    assert(mat_a);
    gsl_matrix *mat_b = gsl_matrix_alloc(MAT_HW, MAT_HW);
    assert(mat_b);

    for (unsigned i = 0; i < MAT_HW; i++)
        for (unsigned j = 0; j < MAT_HW; j++, elem += 1.0) {
            *gsl_matrix_ptr(mat_a, i, j) = elem;
            *gsl_matrix_ptr(mat_b, i, j) = elem + MAT_HW * MAT_HW;            
        }


    gsl_matrix_view mat_a_split[2][2];
    gsl_matrix_view mat_b_split[2][2];

    split_mat(mat_a, mat_a_split);
    split_mat(mat_b, mat_b_split);

    for (unsigned i = 0; i < WORKERS; i++) {
        res = pthread_create(&workers[i], NULL, worker_f, NULL);
        assert(!res);
    }

    clock_t start_time = clock();

    gsl_matrix *mat_res_submat[2][2][2];
    for (unsigned i = 0; i < 2; i++) 
        for (unsigned j = 0; j < 2; j++) {
            MatMul work = {.a = &mat_a_split[i][0].matrix, .b = &mat_b_split[0][j].matrix, .result = &mat_res_submat[i][j][0]};
            semcbuf_push_MatMul(cbuf, &work);
            work = (MatMul){.a = &mat_a_split[i][1].matrix, .b = &mat_b_split[1][j].matrix, .result = &mat_res_submat[i][j][1]};
            semcbuf_push_MatMul(cbuf, &work);
        }

    for (unsigned i = 0; i < WORKERS; i++) {
        // signal each worker to finish
        MatMul poison = {0};
        semcbuf_push_MatMul(cbuf, &poison);
    }

    gsl_matrix *mat_res = gsl_matrix_alloc(MAT_HW, MAT_HW);
    assert(mat_res);

    gsl_matrix_view mat_res_split[2][2];
    split_mat(mat_res, mat_res_split);

    for (int i = 0; i < WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    for (unsigned i = 0; i < 2; i++) 
        for (unsigned j = 0; j < 2; j++) {
            gsl_matrix_memcpy(&mat_res_split[i][j].matrix, mat_res_submat[i][j][0]);
            gsl_matrix_add(&mat_res_split[i][j].matrix, mat_res_submat[i][j][1]);

            gsl_matrix_free(mat_res_submat[i][j][0]);
            gsl_matrix_free(mat_res_submat[i][j][1]);
        }

    clock_t end_time = clock();

    printf("done, needed %.4fs CPU time, now verifying...\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    gsl_matrix *mat_res_ver = gsl_matrix_alloc(MAT_HW, MAT_HW);
    assert(mat_res_ver);

    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, mat_a, mat_b, 0.0, mat_res_ver);

    for (unsigned i = 0; i < MAT_HW; i++)
        for (unsigned j = 0; j < MAT_HW; j++, elem += 1.0) {
            double diff = gsl_matrix_get(mat_res, i, j) - gsl_matrix_get(mat_res_ver, i, j);
            assert(fabs(diff) < 0.001);
        }

    printf("verified OK\n");

    gsl_matrix_free(mat_a);
    gsl_matrix_free(mat_b);
    gsl_matrix_free(mat_res);
    gsl_matrix_free(mat_res_ver);

    semcbuf_deinit_MatMul(cbuf);
    free(cbuf_buf);
    free(cbuf);

    return 0;
}
