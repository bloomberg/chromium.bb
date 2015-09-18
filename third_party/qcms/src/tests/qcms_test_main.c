// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the Chromium LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"
#include "timing.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Manually update the items bellow to add more tests.
extern struct qcms_test_case qcms_test_tetra_clut_rgba_info;
extern struct qcms_test_case qcms_test_data_rgba_out_lut_info;

struct qcms_test_case qcms_test[2];
#define TEST_SIZE    (sizeof(qcms_test) / sizeof(qcms_test[0]))

static void init_tests()
{
    qcms_test[0] = qcms_test_tetra_clut_rgba_info;
    qcms_test[1] = qcms_test_data_rgba_out_lut_info;
}

static void list_tests()
{
    int i;
    printf("Available QCMS tests:\n");

    for (i = 0; i < TEST_SIZE; ++i) {
        printf("\t%s\n", qcms_test[i].test_name);
    }

    exit(0);
}

static void print_usage()
{
    printf("Usage:\n\tqcms_test -w WIDTH -h HEIGHT -n ITERATIONS -t TEST\n");
    printf("\t-w INT\t\tauto-generated image width\n");
    printf("\t-h INT\t\tauto-generated image height\n");
    printf("\t-n INT\t\tnumber of iterations to run the test(s) on\n");
    printf("\t-a\t\trun all tests\n");
    printf("\t-l\t\tlist available tests\n");
    printf("\t-i STRING\tspecify input icc color profile\n");
    printf("\t-o STRING\tspecify output icc color profile\n");
    printf("\t-t STRING\trun specific test - use \"-l\" to list possible values\n");
    printf("\n");
    exit(0);
}

void enable_test(const char *args)
{
    int i;
    for (i = 0; i < TEST_SIZE; ++i) {
        if (strcmp(qcms_test[i].test_name, args) == 0) {
            qcms_test[i].status = QCMS_TEST_ENABLED;
        }
    }
}

void generate_source_uint8_t(unsigned char *src, const size_t length, const size_t pixel_size)
{
    size_t bytes = length * pixel_size;
    size_t i;

    for (i = 0; i < bytes; ++i) {
        *src++ = rand() & 255;
    }
}

int main(int argc, const char **argv)
{
    int iterations = 1;
    size_t height = 2000;
    size_t width = 2000;
    int run_all = 0;
    const char *in = NULL, *out = NULL;

    int i;
    init_tests();

    if (argc == 1) {
        print_usage();
    }

    while (argc > 1) {
        if (strcmp(argv[1], "-n") == 0)
            iterations = abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-w") == 0)
            width = (size_t) abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-h") == 0)
            height = (size_t) abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-l") == 0)
            list_tests();
        else if (strcmp(argv[1], "-t") == 0)
            enable_test(argv[2]);
        else if (strcmp(argv[1], "-a") == 0)
            run_all = 1;
        else if (strcmp(argv[1], "-i") == 0)
            in = argv[2];
        else if (strcmp(argv[1], "-o") == 0)
            out = argv[2];
        (--argc, ++argv);
    }

    for (i = 0; i < TEST_SIZE; ++i) {
        if (run_all || QCMS_TEST_ENABLED == qcms_test[i].status)
            qcms_test[i].test_fn(width, height, iterations, in, out);
    }

    return EXIT_SUCCESS;
}
