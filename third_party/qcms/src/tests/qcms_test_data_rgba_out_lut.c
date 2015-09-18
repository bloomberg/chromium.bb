// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the Chromium LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"
#include "timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void qcms_transform_data_rgba_out_lut_sse2(qcms_transform *transform,
        unsigned char *src,
        unsigned char *dest,
        size_t length,
        qcms_format_type output_format);

extern void qcms_transform_data_rgba_out_lut_precache(qcms_transform *transform,
        unsigned char *src,
        unsigned char *dest,
        size_t length,
        qcms_format_type output_format);

static int diffs;

static int validate(unsigned char *dst0, unsigned char *dst1, size_t length, int limit, const size_t pixel_size)
{
    size_t bytes = length * pixel_size;
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

static int qcms_test_data_rgba_out_lut(size_t width,
        size_t height,
        int iterations,
        const char *in_path,
        const char *out_path)
{
    qcms_profile *in_profile = NULL, *out_profile = NULL;
    qcms_transform *transform0, *transform1;
    qcms_format_type format = {2, 0};

    const size_t pixel_size = 4;
    const size_t length = width * height;

    double time0, time1;
    int i;

    printf("Test qcms data transforms for %d iterations\n", iterations);
    printf("Test image size %u x %u pixels\n", (unsigned) width, (unsigned) height);
    fflush(stdout);

    srand(time(NULL));
    seconds();

    if (in_path == NULL || out_path == NULL) {
        fprintf(stderr, "%s:Please provide valid icc profiles via -i/o option\n", __FUNCTION__);
        return -1;
    }

    in_profile = qcms_profile_from_path(in_path);
    if (!in_profile || qcms_profile_is_bogus(in_profile)) {
        fprintf(stderr, "Invalid input profile\n");
        return -1;
    }

    out_profile = qcms_profile_from_path(out_path);
    if (!out_profile || qcms_profile_is_bogus(out_profile)) {
        fprintf(stderr, "Invalid output profile\n");
        return -1;
    }

    // FIXME(radu.velea): Commenting this and trying qcms_transform_data_rgba_out_lut
    // results in a segfault.
    qcms_profile_precache_output_transform(out_profile);

    transform0 = qcms_transform_create(in_profile, QCMS_DATA_RGBA_8, out_profile, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    transform1 = qcms_transform_create(in_profile, QCMS_DATA_RGBA_8, out_profile, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform0 || !transform1) {
        fprintf(stderr, "Could not create transform, invalid arguments\n");
        return -1;
    }

    transform0->transform_fn = qcms_transform_data_rgba_out_lut_precache;
    transform1->transform_fn = qcms_transform_data_rgba_out_lut_sse2;

    time0 = 0.0;
    time1 = 0.0;

    for (i = 0; i < iterations; ++i) {
        unsigned char *src0 = (unsigned char *)calloc(length, pixel_size);
        unsigned char *src1 = (unsigned char *)calloc(length, pixel_size);
        unsigned char *dst0 = (unsigned char *)calloc(length, pixel_size);
        unsigned char *dst1 = (unsigned char *)calloc(length, pixel_size);

        generate_source_uint8_t(src0, length, pixel_size);
        memcpy(src1, src0, length * pixel_size);

        TIME(transform0->transform_fn(transform0, src0, dst0, length, format), &time0);
        TIME(transform1->transform_fn(transform1, src1, dst1, length, format), &time1);

        if (!validate(dst0, dst1, length, 0, pixel_size)) {
            fprintf(stderr, "Invalid transform output: %d diffs\n", diffs);
        }

        free(src0);
        free(src1);
        free(dst0);
        free(dst1);
    }

    printf("%.6lf (avg %.6lf) seconds qcms_transform_data_rgba_out_lut_precache\n",
            time0, time0 / iterations);
    printf("%.6lf (avg %.6lf) seconds qcms_transform_data_rgba_out_lut_sse2\n",
            time1, time1 / iterations);
    printf("%.6lf speedup after %d iterations\n\n",
            time0 / time1, iterations);

    qcms_transform_release(transform0);
    qcms_transform_release(transform1);

    qcms_profile_release(in_profile);
    qcms_profile_release(out_profile);

    return diffs;
}

struct qcms_test_case qcms_test_data_rgba_out_lut_info = {
        "qcms_test_data_rgba_out_lut",
        qcms_test_data_rgba_out_lut,
        QCMS_TEST_DISABLED
};
