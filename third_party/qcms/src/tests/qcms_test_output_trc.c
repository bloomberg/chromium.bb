// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the Chromium LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PARAMETRIC_CURVE_TYPE 0x70617261 // 'para'

static const float inverse65535 = (float) (1.0 / 65535.0);

extern float clamp_float(float a);

static int get_output_gamma_table(const char *profile_path, uint16_t **table, size_t *size)
{
    qcms_transform *transform;
    qcms_profile *sRGB;
    qcms_profile *target;

    target = qcms_profile_from_path(profile_path);
    if (!target) {
        fprintf(stderr, "Invalid input profile\n");
        return EXIT_FAILURE;
    }

    sRGB = qcms_profile_sRGB();

    transform = qcms_transform_create(sRGB, QCMS_DATA_RGBA_8, target, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform) {
        fprintf(stderr, "Failed to create colour transform\n");
        qcms_profile_release(sRGB);
        qcms_profile_release(target);
        return EXIT_FAILURE;
    }

    *size = qcms_transform_get_output_trc_rgba(transform, target, QCMS_TRC_USHORT, NULL);
    assert(*size == 256);

    *table = malloc(*size * sizeof(uint16_t) * 4);
    qcms_transform_get_output_trc_rgba(transform, target, QCMS_TRC_USHORT, *table);

    qcms_transform_release(transform);
    qcms_profile_release(sRGB);
    qcms_profile_release(target);

    return 0;
}

static int get_input_gamma_table(const char *profile_path, uint16_t **table, size_t *size)
{
    qcms_transform *transform;
    qcms_profile *source;
    qcms_profile *sRGB;

    source = qcms_profile_from_path(profile_path);
    if (!source) {
        fprintf(stderr, "Invalid input profile\n");
        return EXIT_FAILURE;
    }

    sRGB = qcms_profile_sRGB();

    transform = qcms_transform_create(source, QCMS_DATA_RGBA_8, sRGB, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform) {
        fprintf(stderr, "Failed to create colour transform\n");
        qcms_profile_release(sRGB);
        qcms_profile_release(source);
        return EXIT_FAILURE;
    }

    *size = qcms_transform_get_input_trc_rgba(transform, source, QCMS_TRC_USHORT, NULL);
    assert(*size == 256);

    *table = calloc(*size, sizeof(uint16_t) * 4);
    qcms_transform_get_input_trc_rgba(transform, source, QCMS_TRC_USHORT, *table);

    qcms_transform_release(transform);
    qcms_profile_release(sRGB);
    qcms_profile_release(source);

    return 0;
}

static int qcms_test_output_trc(size_t width,
        size_t height,
        int iterations,
        const char *in_path,
        const char *out_path,
        const int force_software)
{
    qcms_profile *profile;
    uint16_t *gamma_table_out;
    size_t gamma_table_size;
    size_t i;

    printf("Test qcms output gamma curve table integrity.\n");

    if (!in_path) {
        fprintf(stderr, "%s: please provide valid ICC profiles via -i option\n", __FUNCTION__);
        return EXIT_FAILURE;
    }

    // Create profiles and transforms, get table and then free resources to make sure none
    // of the internal tables are initialized by previous calls.
    gamma_table_out = NULL;
    gamma_table_size = 0;
    if (get_output_gamma_table(in_path, &gamma_table_out, &gamma_table_size) != 0) {
        fprintf(stderr, "Unable to extract output gamma table\n");
        return EXIT_FAILURE;
    }

    printf("LUT size = %zu\n", gamma_table_size);

    profile = qcms_profile_from_path(in_path);
    if (!profile) {
        fprintf(stderr, "Invalid input profile\n");
        free(gamma_table_out);
        return EXIT_FAILURE;
    }

    // Check only for red curve for now.
    if (profile->redTRC->type == PARAMETRIC_CURVE_TYPE) {
        int type = - (int)(profile->redTRC->count + 1);
        FILE *gamma_file;
        uint16_t *gamma_table_in = NULL;
        uint16_t *p_table_out, *p_table_in;
        char file_name[256] = {0,};
        size_t input_size = 0;

        printf("Detected parametric curve type = %d\n", profile->redTRC->count);

        sprintf(file_name, "qcms-test-%ld-parametric-gamma-%s.csv", (long int)time(NULL), profile->description);
        printf("Writing input and output gamma tables to %s\n", file_name);

        printf("gamma = %.6f, a = %.6f, b = %.6f, c = %.6f, d = %.6f, e = %.6f, f = %.6f\n",
                profile->redTRC->parameter[0], profile->redTRC->parameter[1], profile->redTRC->parameter[2],
                profile->redTRC->parameter[3], profile->redTRC->parameter[4], profile->redTRC->parameter[5],
                profile->redTRC->parameter[6]);

        // Write output to stdout and tables into a csv file.
        gamma_file = fopen(file_name, "w");
        fprintf(gamma_file, "Parametric gamma values for %s\n", profile->description);
        fprintf(gamma_file, "gamma, a, b, c, d, e, f\n");
        fprintf(gamma_file, "%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
                profile->redTRC->parameter[0], profile->redTRC->parameter[1], profile->redTRC->parameter[2],
                profile->redTRC->parameter[3], profile->redTRC->parameter[4], profile->redTRC->parameter[5],
                profile->redTRC->parameter[6]);

        get_input_gamma_table(in_path, &gamma_table_in, &input_size);
        assert(input_size == gamma_table_size);
        if (!gamma_table_in) {
            fprintf(stderr, "Unable to compute input trc. Aborting\n");
            fclose(gamma_file);
            qcms_profile_release(profile);
            free(gamma_table_out);
            return EXIT_FAILURE;
        }

        fprintf(gamma_file, "\n\nInput gamma, Output gamma, LCMS Output gamma, Output gamma error\n");

        p_table_out = gamma_table_out;
        p_table_in = gamma_table_in;

        for (i = 0; i < gamma_table_size; ++i) {
            float p = i / (gamma_table_size * 1.0);
            float reference_out = clamp_float(evaluate_parametric_curve(type, profile->redTRC->parameter, p));
            float actual_out = *p_table_out * inverse65535;
            float error_out = fabs(actual_out - reference_out);
            float input = *p_table_in * inverse65535;

            fprintf(gamma_file, "%.6f, %.6f, %6f, %6f\n",input, actual_out, reference_out, error_out);

            p_table_out += 4; // Skip other channels.
            p_table_in += 4; // Skip other channels.
        }

        free(gamma_table_in);
        fclose(gamma_file);
    }

    qcms_profile_release(profile);
    free(gamma_table_out);

    return 0;
}

struct qcms_test_case qcms_test_output_trc_info = {
        "qcms_test_output_trc",
        qcms_test_output_trc,
        QCMS_TEST_DISABLED
};
