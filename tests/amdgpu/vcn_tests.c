/*
 * Copyright 2017 Advanced Micro Devices, Inc.
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

#include <stdio.h>
#include <inttypes.h>

#include "CUnit/Basic.h"

#include "util_math.h"

#include "amdgpu_test.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"

#define IB_SIZE		4096
#define MAX_RESOURCES	16

struct amdgpu_vcn_bo {
	amdgpu_bo_handle handle;
	amdgpu_va_handle va_handle;
	uint64_t addr;
	uint64_t size;
	uint8_t *ptr;
};

static amdgpu_device_handle device_handle;
static uint32_t major_version;
static uint32_t minor_version;
static uint32_t family_id;

static amdgpu_context_handle context_handle;
static amdgpu_bo_handle ib_handle;
static amdgpu_va_handle ib_va_handle;
static uint64_t ib_mc_address;
static uint32_t *ib_cpu;

static amdgpu_bo_handle resources[MAX_RESOURCES];
static unsigned num_resources;

static void amdgpu_cs_vcn_dec_create(void);
static void amdgpu_cs_vcn_dec_decode(void);
static void amdgpu_cs_vcn_dec_destroy(void);

static void amdgpu_cs_vcn_enc_create(void);
static void amdgpu_cs_vcn_enc_encode(void);
static void amdgpu_cs_vcn_enc_destroy(void);

CU_TestInfo vcn_tests[] = {

	{ "VCN DEC create",  amdgpu_cs_vcn_dec_create },
	{ "VCN DEC decode",  amdgpu_cs_vcn_dec_decode },
	{ "VCN DEC destroy",  amdgpu_cs_vcn_dec_destroy },

	{ "VCN ENC create",  amdgpu_cs_vcn_enc_create },
	{ "VCN ENC decode",  amdgpu_cs_vcn_enc_encode },
	{ "VCN ENC destroy",  amdgpu_cs_vcn_enc_destroy },
	CU_TEST_INFO_NULL,
};

int suite_vcn_tests_init(void)
{
	int r;

	r = amdgpu_device_initialize(drm_amdgpu[0], &major_version,
				     &minor_version, &device_handle);
	if (r)
		return CUE_SINIT_FAILED;

	family_id = device_handle->info.family_id;

	if (family_id < AMDGPU_FAMILY_RV) {
		printf("\n\nThe ASIC NOT support VCN, all sub-tests will pass\n");
		return CUE_SUCCESS;
	}

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	if (r)
		return CUE_SINIT_FAILED;

	r = amdgpu_bo_alloc_and_map(device_handle, IB_SIZE, 4096,
				    AMDGPU_GEM_DOMAIN_GTT, 0,
				    &ib_handle, (void**)&ib_cpu,
				    &ib_mc_address, &ib_va_handle);
	if (r)
		return CUE_SINIT_FAILED;

	return CUE_SUCCESS;
}

int suite_vcn_tests_clean(void)
{
	int r;

	if (family_id < AMDGPU_FAMILY_RV) {
		r = amdgpu_device_deinitialize(device_handle);
		if (r)
			return CUE_SCLEAN_FAILED;
	} else {
		r = amdgpu_bo_unmap_and_free(ib_handle, ib_va_handle,
				     ib_mc_address, IB_SIZE);
		if (r)
			return CUE_SCLEAN_FAILED;

		r = amdgpu_cs_ctx_free(context_handle);
		if (r)
			return CUE_SCLEAN_FAILED;

		r = amdgpu_device_deinitialize(device_handle);
		if (r)
			return CUE_SCLEAN_FAILED;
	}

	return CUE_SUCCESS;
}

static int submit(unsigned ndw, unsigned ip)
{
	struct amdgpu_cs_request ibs_request = {0};
	struct amdgpu_cs_ib_info ib_info = {0};
	struct amdgpu_cs_fence fence_status = {0};
	uint32_t expired;
	int r;

	ib_info.ib_mc_address = ib_mc_address;
	ib_info.size = ndw;

	ibs_request.ip_type = ip;

	r = amdgpu_bo_list_create(device_handle, num_resources, resources,
				  NULL, &ibs_request.resources);
	if (r)
		return r;

	ibs_request.number_of_ibs = 1;
	ibs_request.ibs = &ib_info;
	ibs_request.fence_info.handle = NULL;

	r = amdgpu_cs_submit(context_handle, 0, &ibs_request, 1);
	if (r)
		return r;

	r = amdgpu_bo_list_destroy(ibs_request.resources);
	if (r)
		return r;

	fence_status.context = context_handle;
	fence_status.ip_type = ip;
	fence_status.fence = ibs_request.seq_no;

	r = amdgpu_cs_query_fence_status(&fence_status,
					 AMDGPU_TIMEOUT_INFINITE,
					 0, &expired);
	if (r)
		return r;

	return 0;
}

static void alloc_resource(struct amdgpu_vcn_bo *vcn_bo,
			unsigned size, unsigned domain)
{
	struct amdgpu_bo_alloc_request req = {0};
	amdgpu_bo_handle buf_handle;
	amdgpu_va_handle va_handle;
	uint64_t va = 0;
	int r;

	req.alloc_size = ALIGN(size, 4096);
	req.preferred_heap = domain;
	r = amdgpu_bo_alloc(device_handle, &req, &buf_handle);
	CU_ASSERT_EQUAL(r, 0);
	r = amdgpu_va_range_alloc(device_handle,
				  amdgpu_gpu_va_range_general,
				  req.alloc_size, 1, 0, &va,
				  &va_handle, 0);
	CU_ASSERT_EQUAL(r, 0);
	r = amdgpu_bo_va_op(buf_handle, 0, req.alloc_size, va, 0,
			    AMDGPU_VA_OP_MAP);
	CU_ASSERT_EQUAL(r, 0);
	vcn_bo->addr = va;
	vcn_bo->handle = buf_handle;
	vcn_bo->size = req.alloc_size;
	vcn_bo->va_handle = va_handle;
	r = amdgpu_bo_cpu_map(vcn_bo->handle, (void **)&vcn_bo->ptr);
	CU_ASSERT_EQUAL(r, 0);
	memset(vcn_bo->ptr, 0, size);
	r = amdgpu_bo_cpu_unmap(vcn_bo->handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void free_resource(struct amdgpu_vcn_bo *vcn_bo)
{
	int r;

	r = amdgpu_bo_va_op(vcn_bo->handle, 0, vcn_bo->size,
			    vcn_bo->addr, 0, AMDGPU_VA_OP_UNMAP);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_va_range_free(vcn_bo->va_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_free(vcn_bo->handle);
	CU_ASSERT_EQUAL(r, 0);
	memset(vcn_bo, 0, sizeof(*vcn_bo));
}

static void amdgpu_cs_vcn_dec_create(void)
{
	if (family_id < AMDGPU_FAMILY_RV)
		return;

	/* TODO */
}

static void amdgpu_cs_vcn_dec_decode(void)
{
	if (family_id < AMDGPU_FAMILY_RV)
		return;

	/* TODO */
}

static void amdgpu_cs_vcn_dec_destroy(void)
{
	if (family_id < AMDGPU_FAMILY_RV)
		return;

	/* TODO */
}

static void amdgpu_cs_vcn_enc_create(void)
{
	if (family_id < AMDGPU_FAMILY_RV)
		return;

	/* TODO */
}

static void amdgpu_cs_vcn_enc_encode(void)
{
	if (family_id < AMDGPU_FAMILY_RV)
		return;

	/* TODO */
}

static void amdgpu_cs_vcn_enc_destroy(void)
{
	if (family_id < AMDGPU_FAMILY_RV)
		return;

	/* TODO */
}
