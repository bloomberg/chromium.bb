/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdint.h>

#include "drm.h"
#include "xf86drmMode.h"
#include "xf86drm.h"

#include "CUnit/Basic.h"

#include "amdgpu_test.h"

/**
 *  Open handles for amdgpu devices
 *
 */
int drm_amdgpu[MAX_CARDS_SUPPORTED];

/** The table of all known test suites to run */
static CU_SuiteInfo suites[] = {
	{
		.pName = "Basic Tests",
		.pInitFunc = suite_basic_tests_init,
		.pCleanupFunc = suite_basic_tests_clean,
		.pTests = basic_tests,
	},
	{
		.pName = "BO Tests",
		.pInitFunc = suite_bo_tests_init,
		.pCleanupFunc = suite_bo_tests_clean,
		.pTests = bo_tests,
	},
	{
		.pName = "CS Tests",
		.pInitFunc = suite_cs_tests_init,
		.pCleanupFunc = suite_cs_tests_clean,
		.pTests = cs_tests,
	},
	{
		.pName = "VCE Tests",
		.pInitFunc = suite_vce_tests_init,
		.pCleanupFunc = suite_vce_tests_clean,
		.pTests = vce_tests,
	},
	CU_SUITE_INFO_NULL,
};


/** Display information about all  suites and their tests */
static void display_test_suites(void)
{
	int iSuite;
	int iTest;

	printf("Suites\n");

	for (iSuite = 0; suites[iSuite].pName != NULL; iSuite++) {
		printf("Suite id = %d: Name '%s'\n",
				iSuite + 1, suites[iSuite].pName);

		for (iTest = 0; suites[iSuite].pTests[iTest].pName != NULL;
			iTest++) {
			printf("	Test id %d: Name: '%s'\n", iTest + 1,
					suites[iSuite].pTests[iTest].pName);
		}
	}
}


/** Help string for command line parameters */
static const char usage[] = "Usage: %s [-hl] [<-s <suite id>> [-t <test id>]]\n"
				"where:\n"
				"       l - Display all suites and their tests\n"
				"       h - Display this help\n";
/** Specified options strings for getopt */
static const char options[]   = "hls:t:";

/* The main() function for setting up and running the tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main(int argc, char **argv)
{
	int c;			/* Character received from getopt */
	int i = 0;
	int suite_id = -1;	/* By default run everything */
	int test_id  = -1;	/* By default run all tests in the suite */
	CU_pSuite pSuite = NULL;
	CU_pTest  pTest  = NULL;

	int aval = drmAvailable();

	if (aval == 0) {
		fprintf(stderr, "DRM driver is not available\n");
		exit(EXIT_FAILURE);
	}


	for (i = 0; i < MAX_CARDS_SUPPORTED; i++)
		drm_amdgpu[i] = -1;


	/* Parse command line string */
	opterr = 0;		/* Do not print error messages from getopt */
	while ((c = getopt(argc, argv, options)) != -1) {
		switch (c) {
		case 'l':
			display_test_suites();
			exit(EXIT_SUCCESS);
		case 's':
			suite_id = atoi(optarg);
			break;
		case 't':
			test_id = atoi(optarg);
			break;
		case '?':
		case 'h':
			fprintf(stderr, usage, argv[0]);
			exit(EXIT_SUCCESS);
		default:
			fprintf(stderr, usage, argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	/* Try to open all possible radeon connections
	 * Right now: Open only the 0.
	 */
	printf("Try to open the card 0..\n");
	drm_amdgpu[0] = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

	if (drm_amdgpu[0] < 0) {
		perror("Cannot open /dev/dri/card0\n");
		exit(EXIT_FAILURE);
	}

	/** Display version of DRM driver */
	drmVersionPtr retval = drmGetVersion(drm_amdgpu[0]);

	if (retval == NULL) {
		perror("Could not get information about DRM driver");
		exit(EXIT_FAILURE);
	}

	printf("DRM Driver: Name: [%s] : Date [%s] : Description [%s]\n",
	       retval->name, retval->date, retval->desc);

	drmFreeVersion(retval);

	/* Initialize test suites to run */

	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry()) {
		close(drm_amdgpu[0]);
		return CU_get_error();
	}

	/* Register suites. */
	if (CU_register_suites(suites) != CUE_SUCCESS) {
		fprintf(stderr, "suite registration failed - %s\n",
				CU_get_error_msg());
		CU_cleanup_registry();
		close(drm_amdgpu[0]);
		exit(EXIT_FAILURE);
	}

	/* Run tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);

	if (suite_id != -1) {	/* If user specify particular suite? */
		pSuite = CU_get_suite_by_index((unsigned int) suite_id,
						CU_get_registry());

		if (pSuite) {
			if (test_id != -1) {   /* If user specify test id */
				pTest = CU_get_test_by_index(
						(unsigned int) test_id,
						pSuite);
				if (pTest)
					CU_basic_run_test(pSuite, pTest);
				else {
					fprintf(stderr, "Invalid test id: %d\n",
								test_id);
					CU_cleanup_registry();
					close(drm_amdgpu[0]);
					exit(EXIT_FAILURE);
				}
			} else
				CU_basic_run_suite(pSuite);
		} else {
			fprintf(stderr, "Invalid suite id : %d\n",
					suite_id);
			CU_cleanup_registry();
			close(drm_amdgpu[0]);
			exit(EXIT_FAILURE);
		}
	} else
		CU_basic_run_tests();

	CU_cleanup_registry();
	close(drm_amdgpu[0]);
	return CU_get_error();
}
