// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the Chromium LICENSE file.

#include "qcms.h"
#include "qcmsint.h"
#include "qcmstypes.h"
#include "timing.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External qcms functions being tested.
extern void qcms_transform_data_tetra_clut_rgba(qcms_transform *transform,
                                                unsigned char *src,
                                                unsigned char *dest,
                                                size_t length,
                                                qcms_format_type output_format);

extern void qcms_transform_data_tetra_clut_rgba_sse2(qcms_transform *transform,
                                                     unsigned char *src,
                                                     unsigned char *dest,
                                                     size_t length,
                                                     qcms_format_type output_format);

#define PIXEL_SIZE    4

static void generate_source(unsigned char *src, int length) {
    int i;
    int bytes = length * PIXEL_SIZE;

    for (i = 0; i < bytes; ++i) {
        *src++ = rand() & 255;
    }
}

static float *create_lut(int lutSize) {
    float *lut = malloc(lutSize * sizeof(float));
    int i;

    for (i = 0; i < lutSize; i++) {
        lut[i] = (rand() & 255) * (1.0f / 255.0f);
    }

    return lut;
}

static int validate(unsigned char *dst0, unsigned char *dst1, int length) {
    int i;
    int bytes = length * PIXEL_SIZE;

    for (i = 0; i < bytes; ++i) {
        // This should account for differences resulted from rounding operations.
        if (abs((int)dst0[i] - (int)dst1[i]) > 1) {
            return -1;
        }
    }

    return 0;
}

int main(int argc, const char **args) {
    qcms_transform transform0, transform1;
    qcms_format_type output_format = {2, 0};
    int samples = 33;
    uint32_t lutSize;
    float *lut0, *lut1;

    size_t length = 2000;
    double time0, time1;
    int i;
    int iterations = 1000;

    // Allow testing with different number of iterations to reduce measurement noise.
    if (argc > 1) {
        iterations = atoi(args[1]);
    }

    printf("Running qcms_transform_data_tetra_clut_rgba test for %d iterations\n", iterations);
    fflush(stdout);

    srand(0);
    seconds();

    transform0.grid_size = samples;
    transform1.grid_size = samples;

    lutSize = 3 * samples * samples * samples;
    lut0 = create_lut(lutSize);
    lut1 = (float *)malloc(lutSize * sizeof(float));
    memcpy(lut1, lut0, lutSize * sizeof(float));

    transform0.r_clut = &lut0[0];
    transform0.g_clut = &lut0[1];
    transform0.b_clut = &lut0[2];

    transform1.r_clut = &lut1[0];
    transform1.g_clut = &lut1[1];
    transform1.b_clut = &lut1[2];

    time0 = 0.0;
    time1 = 0.0;

    // Re-generate and use different data sources the during iteration loop
    // to avoid caching or compiler optimizations that may affect performance.
	
    for (i = 0; i < iterations; ++i) {
        unsigned char *src0 = (unsigned char *)calloc(length, PIXEL_SIZE);
        unsigned char *dst0 = (unsigned char *)calloc(length, PIXEL_SIZE);
        unsigned char *src1 = (unsigned char *)calloc(length, PIXEL_SIZE);
        unsigned char *dst1 = (unsigned char *)calloc(length, PIXEL_SIZE);

        generate_source(src0, length);
        memcpy(src1, src0, length * PIXEL_SIZE);

        TIME(qcms_transform_data_tetra_clut_rgba(&transform0, src0, dst0, length, output_format), &time0);
        TIME(qcms_transform_data_tetra_clut_rgba_sse2(&transform1, src1, dst1, length, output_format), &time1);

        if (validate(dst0, dst1, length) != 0) {
            fprintf(stderr, "Failed to validate output!\n");
            return EXIT_FAILURE;
        }

        free(src0);
        free(src1);
        free(dst0);
        free(dst1);
    }

    printf("Test Results:\n");
    printf("%.6lf seconds for qcms_transform_data_tetra_clut_rgba\n", time0);
    printf("%.6lf seconds for qcms_transform_data_tetra_clut_rgba_sse2\n", time1);
    printf("%.6lf speedup after %d iterations\n\n", time0 / time1, iterations);

    free(lut0);
    free(lut1);

    return EXIT_SUCCESS;
}
