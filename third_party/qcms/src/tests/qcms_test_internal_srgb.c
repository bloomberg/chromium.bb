// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the Chromium LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static s15Fixed16Number sRGB_reference[3][3] = {
		{0x06fa2 /*0.436066*/,  /*0.385147*/ 0x06299, /*0.143066*/ 0x024a0},
		{0x038f5 /*0.222488*/,  /*0.716873*/ 0x0b785, /*0.060608*/ 0x00f84},
		{0x00390 /*0.013916*/,  /*0.097076*/ 0x018da, /*0.714096*/ 0x0b6cf}};

static int qcms_test_internal_srgb(size_t width,
		size_t height,
		int iterations,
		const char *in_path,
		const char *out_path,
		const int force_software)
{
	qcms_profile *profile = qcms_profile_sRGB();
	s15Fixed16Number sRGB_internal[3][3];
	s15Fixed16Number diff = 0;
	int i, j;

	printf("Test qcms internal sRGB colorant matrix against official sRGB IEC61966-2.1 color (D50) primaries\n");

	sRGB_internal[0][0] = profile->redColorant.X;
	sRGB_internal[1][0] = profile->redColorant.Y;
	sRGB_internal[2][0] = profile->redColorant.Z;
	sRGB_internal[0][1] = profile->greenColorant.X;
	sRGB_internal[1][1] = profile->greenColorant.Y;
	sRGB_internal[2][1] = profile->greenColorant.Z;
	sRGB_internal[0][2] = profile->blueColorant.X;
	sRGB_internal[1][2] = profile->blueColorant.Y;
	sRGB_internal[2][2] = profile->blueColorant.Z;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			s15Fixed16Number tmp = sRGB_internal[i][j] - sRGB_reference[i][j];
			printf("\t%d", tmp);
			diff += abs(tmp);
		}
		printf("\n");
	}

	qcms_profile_release(profile);

	printf("Total error = 0x%x [%.6f]\n", diff, diff / 65536.0);

	return diff;
}

struct qcms_test_case qcms_test_internal_srgb_info = {
		"qcms_test_internal_srgb",
		qcms_test_internal_srgb,
		QCMS_TEST_DISABLED
};
