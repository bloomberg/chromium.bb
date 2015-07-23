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

// External qcms tetra clut interpolators.

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

#define PIXEL_SIZE 4

static void generate_source(unsigned char *src, size_t length)
{
    size_t bytes = length * PIXEL_SIZE;
    size_t i;

    for (i = 0; i < bytes; ++i) {
        *src++ = rand() & 255;
    }
}

static float *create_lut(size_t lutSize)
{
    float *lut = malloc(lutSize * sizeof(float));
    size_t i;

    for (i = 0; i < lutSize; ++i) {
        lut[i] = (rand() & 255) * (1.0f / 255.0f);
    }

    return lut;
}

static int diffs;

static int validate(unsigned char *dst0, unsigned char *dst1, size_t length, int limit)
{
    size_t bytes = length * PIXEL_SIZE;
    size_t i;

    // Compare dst0/dst0 byte-by-byte, allowing for minor differences due
    // to SSE rounding modes (controlled by the limit argument).

    if (limit < 0)
        limit = 255; // Ignore all differences.

    for (diffs = 0, i = 0; i < bytes; ++i) {
        if (abs((int)dst0[i] - (int)dst1[i]) > limit) {
            ++diffs;
        }
    }

    return !diffs;
}

int main(int argc, const char **argv)
{
    qcms_transform transform0, transform1;
    qcms_format_type format = {2, 0};
    uint16_t samples = 33;
    size_t lutSize;
    float *lut0, *lut1;

    int iterations = 1;
    size_t height = 2000;
    size_t width = 2000;
    size_t length;

    double time0, time1;
    int i;

    while (argc > 1) {
        if (strcmp(argv[1], "-i") == 0)
            iterations = abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-w") == 0)
            width = (size_t) abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-h") == 0)
            height = (size_t) abs(atoi(argv[2]));
        (--argc, ++argv);
    }

    printf("Test qcms clut transforms for %d iterations\n", iterations);
    printf("Test image size %u x %u pixels\n", (unsigned) width, (unsigned) height);
    length = width * height;
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

    // Re-generate and use different data sources during the iteration loop
    // to avoid compiler / cache optimizations that may affect performance.

    time0 = 0.0;
    time1 = 0.0;

    for (i = 0; i < iterations; ++i) {
        unsigned char *src0 = (unsigned char *)calloc(length, PIXEL_SIZE);
        unsigned char *src1 = (unsigned char *)calloc(length, PIXEL_SIZE);
        unsigned char *dst0 = (unsigned char *)calloc(length, PIXEL_SIZE);
        unsigned char *dst1 = (unsigned char *)calloc(length, PIXEL_SIZE);

        generate_source(src0, length);
        memcpy(src1, src0, length * PIXEL_SIZE);

#define TRANSFORM_TEST0 qcms_transform_data_tetra_clut_rgba
#define TRANSFORM_TEST1 qcms_transform_data_tetra_clut_rgba_sse2

        TIME(TRANSFORM_TEST0(&transform0, src0, dst0, length, format), &time0);
        TIME(TRANSFORM_TEST1(&transform1, src1, dst1, length, format), &time1);

        if (!validate(dst0, dst1, length, 0)) {
            fprintf(stderr, "Invalid transform output: %d diffs\n", diffs);
        }

        free(src0);
        free(src1);
        free(dst0);
        free(dst1);
    }

#define STRINGIZE(s) #s
#define STRING(s) STRINGIZE(s)

    printf("%.6lf (avg %.6lf) seconds " STRING(TRANSFORM_TEST0) "\n",
           time0, time0 / iterations);
    printf("%.6lf (avg %.6lf) seconds " STRING(TRANSFORM_TEST1) "\n",
           time1, time1 / iterations);
    printf("%.6lf speedup after %d iterations\n\n",
           time0 / time1, iterations);

    free(lut0);
    free(lut1);

    return EXIT_SUCCESS;
}
