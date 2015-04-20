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

#ifndef _AMDGPU_TEST_H_
#define _AMDGPU_TEST_H_

#include "amdgpu.h"

/**
 * Define max. number of card in system which we are able to handle
 */
#define MAX_CARDS_SUPPORTED     4

/* Forward reference for array to keep "drm" handles */
extern int drm_amdgpu[MAX_CARDS_SUPPORTED];

/*************************  Basic test suite ********************************/

/*
 * Define basic test suite to serve as the starting point for future testing
*/

/**
 * Initialize basic test suite
 */
int suite_basic_tests_init();

/**
 * Deinitialize basic test suite
 */
int suite_basic_tests_clean();

/**
 * Tests in basic test suite
 */
extern CU_TestInfo basic_tests[];

/**
 * Initialize bo test suite
 */
int suite_bo_tests_init();

/**
 * Deinitialize bo test suite
 */
int suite_bo_tests_clean();

/**
 * Tests in bo test suite
 */
extern CU_TestInfo bo_tests[];

/**
 * Initialize cs test suite
 */
int suite_cs_tests_init();

/**
 * Deinitialize cs test suite
 */
int suite_cs_tests_clean();

/**
 * Tests in cs test suite
 */
extern CU_TestInfo cs_tests[];

/**
 * Helper functions
 */
static inline amdgpu_bo_handle gpu_mem_alloc(
					amdgpu_device_handle device_handle,
					uint64_t size,
					uint64_t alignment,
					uint32_t type,
					uint64_t flags,
					uint64_t *vmc_addr)
{
	struct amdgpu_bo_alloc_request req = {0};
	struct amdgpu_bo_alloc_result res = {0};
	int r;

	CU_ASSERT_NOT_EQUAL(vmc_addr, NULL);

	req.alloc_size = size;
	req.phys_alignment = alignment;
	req.preferred_heap = type;
	req.flags = flags;

	r = amdgpu_bo_alloc(device_handle, &req, &res);
	CU_ASSERT_EQUAL(r, 0);

	CU_ASSERT_NOT_EQUAL(res.virtual_mc_base_address, 0);
	CU_ASSERT_NOT_EQUAL(res.buf_handle, NULL);
	*vmc_addr = res.virtual_mc_base_address;
	return res.buf_handle;
}

#endif  /* #ifdef _AMDGPU_TEST_H_ */
