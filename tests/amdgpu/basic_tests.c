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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "CUnit/Basic.h"

#include "amdgpu_test.h"
#include "amdgpu_drm.h"

static  amdgpu_device_handle device_handle;
static  uint32_t  major_version;
static  uint32_t  minor_version;

static void amdgpu_query_info_test(void);
static void amdgpu_memory_alloc(void);
static void amdgpu_command_submission_gfx(void);
static void amdgpu_command_submission_compute(void);
static void amdgpu_command_submission_sdma(void);
static void amdgpu_userptr_test(void);

CU_TestInfo basic_tests[] = {
	{ "Query Info Test",  amdgpu_query_info_test },
	{ "Memory alloc Test",  amdgpu_memory_alloc },
	{ "Userptr Test",  amdgpu_userptr_test },
	{ "Command submission Test (GFX)",  amdgpu_command_submission_gfx },
	{ "Command submission Test (Compute)", amdgpu_command_submission_compute },
	{ "Command submission Test (SDMA)", amdgpu_command_submission_sdma },
	CU_TEST_INFO_NULL,
};
#define BUFFER_SIZE (8 * 1024)
#define SDMA_PKT_HEADER_op_offset 0
#define SDMA_PKT_HEADER_op_mask   0x000000FF
#define SDMA_PKT_HEADER_op_shift  0
#define SDMA_PKT_HEADER_OP(x) (((x) & SDMA_PKT_HEADER_op_mask) << SDMA_PKT_HEADER_op_shift)
#define SDMA_OPCODE_CONSTANT_FILL  11
#       define SDMA_CONSTANT_FILL_EXTRA_SIZE(x)           ((x) << 14)
	/* 0 = byte fill
	 * 2 = DW fill
	 */
#define SDMA_PACKET(op, sub_op, e)	((((e) & 0xFFFF) << 16) |	\
					(((sub_op) & 0xFF) << 8) |	\
					(((op) & 0xFF) << 0))
#define	SDMA_OPCODE_WRITE				  2
#       define SDMA_WRITE_SUB_OPCODE_LINEAR               0
#       define SDMA_WRTIE_SUB_OPCODE_TILED                1

#define	SDMA_OPCODE_COPY				  1
#       define SDMA_COPY_SUB_OPCODE_LINEAR                0

int suite_basic_tests_init(void)
{
	int r;

	r = amdgpu_device_initialize(drm_amdgpu[0], &major_version,
				   &minor_version, &device_handle);

	if (r == 0)
		return CUE_SUCCESS;
	else
		return CUE_SINIT_FAILED;
}

int suite_basic_tests_clean(void)
{
	int r = amdgpu_device_deinitialize(device_handle);

	if (r == 0)
		return CUE_SUCCESS;
	else
		return CUE_SCLEAN_FAILED;
}

static void amdgpu_query_info_test(void)
{
	struct amdgpu_gpu_info gpu_info = {0};
	uint32_t version, feature;
	int r;

	r = amdgpu_query_gpu_info(device_handle, &gpu_info);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_query_firmware_version(device_handle, AMDGPU_INFO_FW_VCE, 0,
					  0, &version, &feature);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_memory_alloc(void)
{
	amdgpu_bo_handle bo;
	uint64_t bo_mc;
	int r;

	/* Test visible VRAM */
	bo = gpu_mem_alloc(device_handle,
			4096, 4096,
			AMDGPU_GEM_DOMAIN_VRAM,
			AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
			&bo_mc);

	r = amdgpu_bo_free(bo);
	CU_ASSERT_EQUAL(r, 0);

	/* Test invisible VRAM */
	bo = gpu_mem_alloc(device_handle,
			4096, 4096,
			AMDGPU_GEM_DOMAIN_VRAM,
			AMDGPU_GEM_CREATE_NO_CPU_ACCESS,
			&bo_mc);

	r = amdgpu_bo_free(bo);
	CU_ASSERT_EQUAL(r, 0);

	/* Test GART Cacheable */
	bo = gpu_mem_alloc(device_handle,
			4096, 4096,
			AMDGPU_GEM_DOMAIN_GTT,
			0,
			&bo_mc);

	r = amdgpu_bo_free(bo);
	CU_ASSERT_EQUAL(r, 0);

	/* Test GART USWC */
	bo = gpu_mem_alloc(device_handle,
			4096, 4096,
			AMDGPU_GEM_DOMAIN_GTT,
			AMDGPU_GEM_CREATE_CPU_GTT_WC,
			&bo_mc);

	r = amdgpu_bo_free(bo);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_command_submission_gfx(void)
{
	amdgpu_context_handle context_handle;
	struct amdgpu_cs_ib_alloc_result ib_result = {0};
	struct amdgpu_cs_ib_alloc_result ib_result_ce = {0};
	struct amdgpu_cs_request ibs_request = {0};
	struct amdgpu_cs_ib_info ib_info[2];
	struct amdgpu_cs_query_fence fence_status = {0};
	uint32_t *ptr;
	uint32_t expired;
	int r;

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_alloc_ib(device_handle, context_handle,
			       amdgpu_cs_ib_size_4K, &ib_result);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_alloc_ib(device_handle, context_handle,
			       amdgpu_cs_ib_size_4K, &ib_result_ce);
	CU_ASSERT_EQUAL(r, 0);

	memset(ib_info, 0, 2 * sizeof(struct amdgpu_cs_ib_info));

	/* IT_SET_CE_DE_COUNTERS */
	ptr = ib_result_ce.cpu;
	ptr[0] = 0xc0008900;
	ptr[1] = 0;
	ptr[2] = 0xc0008400;
	ptr[3] = 1;
	ib_info[0].ib_handle = ib_result_ce.handle;
	ib_info[0].size = 4;
	ib_info[0].flags = AMDGPU_CS_GFX_IB_CE;

	/* IT_WAIT_ON_CE_COUNTER */
	ptr = ib_result.cpu;
	ptr[0] = 0xc0008600;
	ptr[1] = 0x00000001;
	ib_info[1].ib_handle = ib_result.handle;
	ib_info[1].size = 2;

	ibs_request.ip_type = AMDGPU_HW_IP_GFX;
	ibs_request.number_of_ibs = 2;
	ibs_request.ibs = ib_info;

	r = amdgpu_cs_submit(device_handle, context_handle, 0,
			     &ibs_request, 1, &fence_status.fence);
	CU_ASSERT_EQUAL(r, 0);

	fence_status.context = context_handle;
	fence_status.timeout_ns = AMDGPU_TIMEOUT_INFINITE;
	fence_status.ip_type = AMDGPU_HW_IP_GFX;

	r = amdgpu_cs_query_fence_status(device_handle, &fence_status, &expired);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_ctx_free(device_handle, context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_command_submission_compute(void)
{
	amdgpu_context_handle context_handle;
	struct amdgpu_cs_ib_alloc_result ib_result;
	struct amdgpu_cs_request ibs_request;
	struct amdgpu_cs_ib_info ib_info;
	struct amdgpu_cs_query_fence fence_status;
	uint32_t *ptr;
	uint32_t expired;
	int i, r, instance;

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	for (instance = 0; instance < 8; instance++) {
		memset(&ib_result, 0, sizeof(struct amdgpu_cs_ib_alloc_result));
		r = amdgpu_cs_alloc_ib(device_handle, context_handle,
				       amdgpu_cs_ib_size_4K, &ib_result);
		CU_ASSERT_EQUAL(r, 0);

		ptr = ib_result.cpu;
		for (i = 0; i < 16; ++i)
			ptr[i] = 0xffff1000;

		memset(&ib_info, 0, sizeof(struct amdgpu_cs_ib_info));
		ib_info.ib_handle = ib_result.handle;
		ib_info.size = 16;

		memset(&ibs_request, 0, sizeof(struct amdgpu_cs_request));
		ibs_request.ip_type = AMDGPU_HW_IP_COMPUTE;
		ibs_request.ring = instance;
		ibs_request.number_of_ibs = 1;
		ibs_request.ibs = &ib_info;

		memset(&fence_status, 0, sizeof(struct amdgpu_cs_query_fence));
		r = amdgpu_cs_submit(device_handle, context_handle, 0,
				     &ibs_request, 1, &fence_status.fence);
		CU_ASSERT_EQUAL(r, 0);

		fence_status.context = context_handle;
		fence_status.timeout_ns = AMDGPU_TIMEOUT_INFINITE;
		fence_status.ip_type = AMDGPU_HW_IP_COMPUTE;
		fence_status.ring = instance;

		r = amdgpu_cs_query_fence_status(device_handle, &fence_status, &expired);
		CU_ASSERT_EQUAL(r, 0);
	}

	r = amdgpu_cs_ctx_free(device_handle, context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

/*
 * caller need create/release:
 * pm4_src, resources, ib_info, and ibs_request
 * submit command stream described in ibs_request and wait for this IB accomplished
 */
static void amdgpu_sdma_test_exec_cs(amdgpu_context_handle context_handle,
				 int instance, int pm4_dw, uint32_t *pm4_src,
				 int res_cnt, amdgpu_bo_handle *resources,
				 struct amdgpu_cs_ib_info *ib_info,
				 struct amdgpu_cs_request *ibs_request)
{
	int r, i, j;
	uint32_t expired;
	uint32_t *ring_ptr;
	struct amdgpu_cs_ib_alloc_result ib_result = {0};
	struct amdgpu_cs_query_fence fence_status = {0};

	/* prepare CS */
	CU_ASSERT_NOT_EQUAL(pm4_src, NULL);
	CU_ASSERT_NOT_EQUAL(resources, NULL);
	CU_ASSERT_NOT_EQUAL(ib_info, NULL);
	CU_ASSERT_NOT_EQUAL(ibs_request, NULL);
	CU_ASSERT_TRUE(pm4_dw <= 1024);

	/* allocate IB */
	r = amdgpu_cs_alloc_ib(device_handle, context_handle,
			   amdgpu_cs_ib_size_4K, &ib_result);
	CU_ASSERT_EQUAL(r, 0);

	/* copy PM4 packet to ring from caller */
	ring_ptr = ib_result.cpu;
	memcpy(ring_ptr, pm4_src, pm4_dw * sizeof(*pm4_src));

	ib_info->ib_handle = ib_result.handle;
	ib_info->size = pm4_dw;

	ibs_request->ip_type = AMDGPU_HW_IP_DMA;
	ibs_request->ring = instance;
	ibs_request->number_of_ibs = 1;
	ibs_request->ibs = ib_info;
	ibs_request->number_of_resources = res_cnt;
	ibs_request->resources = resources;


	CU_ASSERT_NOT_EQUAL(ibs_request, NULL);

	/* submit CS */
	r = amdgpu_cs_submit(device_handle, context_handle, 0,
			 ibs_request, 1, &fence_status.fence);
	CU_ASSERT_EQUAL(r, 0);

	fence_status.ip_type = AMDGPU_HW_IP_DMA;
	fence_status.ring = ibs_request->ring;
	fence_status.context = context_handle;
	fence_status.timeout_ns = AMDGPU_TIMEOUT_INFINITE;

	/* wait for IB accomplished */
	r = amdgpu_cs_query_fence_status(device_handle, &fence_status,
					&expired);
	CU_ASSERT_EQUAL(r, 0);
	CU_ASSERT_EQUAL(expired, true);
}

static void amdgpu_command_submission_sdma_write_linear(void)
{
	const int sdma_write_length = 128;
	const int pm4_dw = 256;
	amdgpu_context_handle context_handle;
	amdgpu_bo_handle bo;
	amdgpu_bo_handle *resources;
	uint32_t *pm4;
	struct amdgpu_cs_ib_info *ib_info;
	struct amdgpu_cs_request *ibs_request;
	uint64_t bo_mc;
	volatile uint32_t *bo_cpu;
	int i, j, r, loop;
	uint64_t gtt_flags[3] = {0, AMDGPU_GEM_CREATE_CPU_GTT_UC,
				AMDGPU_GEM_CREATE_CPU_GTT_WC};

	pm4 = calloc(pm4_dw, sizeof(*pm4));
	CU_ASSERT_NOT_EQUAL(pm4, NULL);

	ib_info = calloc(1, sizeof(*ib_info));
	CU_ASSERT_NOT_EQUAL(ib_info, NULL);

	ibs_request = calloc(1, sizeof(*ibs_request));
	CU_ASSERT_NOT_EQUAL(ibs_request, NULL);

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	/* prepare resource */
	resources = calloc(1, sizeof(amdgpu_bo_handle));
	CU_ASSERT_NOT_EQUAL(resources, NULL);

	loop = 0;
	while(loop < 3) {
		/* allocate UC bo for sDMA use */
		bo = gpu_mem_alloc(device_handle,
				sdma_write_length * sizeof(uint32_t),
				4096, AMDGPU_GEM_DOMAIN_GTT,
				gtt_flags[loop], &bo_mc);

		CU_ASSERT_EQUAL(amdgpu_bo_cpu_map(bo, (void **)&bo_cpu), 0);
		CU_ASSERT_NOT_EQUAL(bo_cpu, NULL);

		/* clear bo */
		memset((void*)bo_cpu, 0, sdma_write_length * sizeof(uint32_t));


		resources[0] = bo;

		/* fullfill PM4: test DMA write-linear */
		i = j = 0;
		pm4[i++] = SDMA_PACKET(SDMA_OPCODE_WRITE,
				SDMA_WRITE_SUB_OPCODE_LINEAR, 0);
		pm4[i++] = 0xffffffff & bo_mc;
		pm4[i++] = (0xffffffff00000000 & bo_mc) >> 32;
		pm4[i++] = sdma_write_length;
		while(j++ < sdma_write_length)
			pm4[i++] = 0xdeadbeaf;

		amdgpu_sdma_test_exec_cs(context_handle, 0,
					i, pm4,
					1, resources,
					ib_info, ibs_request);

		/* verify if SDMA test result meets with expected */
		i = 0;
		while(i < sdma_write_length) {
			CU_ASSERT_EQUAL(bo_cpu[i++], 0xdeadbeaf);
		}
		amdgpu_bo_free(bo);
		loop++;
	}
	/* clean resources */
	free(resources);
	free(ibs_request);
	free(ib_info);
	free(pm4);

	/* end of test */
	r = amdgpu_cs_ctx_free(device_handle, context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_command_submission_sdma_const_fill(void)
{
	const int sdma_write_length = 1024 * 1024;
	const int pm4_dw = 256;
	amdgpu_context_handle context_handle;
	amdgpu_bo_handle bo;
	amdgpu_bo_handle *resources;
	uint32_t *pm4;
	struct amdgpu_cs_ib_info *ib_info;
	struct amdgpu_cs_request *ibs_request;
	uint64_t bo_mc;
	volatile uint32_t *bo_cpu;
	int i, j, r, loop;
	uint64_t gtt_flags[3] = {0, AMDGPU_GEM_CREATE_CPU_GTT_UC,
				AMDGPU_GEM_CREATE_CPU_GTT_WC};

	pm4 = calloc(pm4_dw, sizeof(*pm4));
	CU_ASSERT_NOT_EQUAL(pm4, NULL);

	ib_info = calloc(1, sizeof(*ib_info));
	CU_ASSERT_NOT_EQUAL(ib_info, NULL);

	ibs_request = calloc(1, sizeof(*ibs_request));
	CU_ASSERT_NOT_EQUAL(ibs_request, NULL);

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	/* prepare resource */
	resources = calloc(1, sizeof(amdgpu_bo_handle));
	CU_ASSERT_NOT_EQUAL(resources, NULL);

	loop = 0;
	while(loop < 3) {
		/* allocate UC bo for sDMA use */
		bo = gpu_mem_alloc(device_handle,
				sdma_write_length, 4096,
				AMDGPU_GEM_DOMAIN_GTT,
				gtt_flags[loop], &bo_mc);

		CU_ASSERT_EQUAL(amdgpu_bo_cpu_map(bo, (void **)&bo_cpu), 0);
		CU_ASSERT_NOT_EQUAL(bo_cpu, NULL);

		/* clear bo */
		memset((void*)bo_cpu, 0, sdma_write_length);

		resources[0] = bo;

		/* fullfill PM4: test DMA const fill */
		i = j = 0;
		pm4[i++] = SDMA_PACKET(SDMA_OPCODE_CONSTANT_FILL, 0,
				   SDMA_CONSTANT_FILL_EXTRA_SIZE(2));
		pm4[i++] = 0xffffffff & bo_mc;
		pm4[i++] = (0xffffffff00000000 & bo_mc) >> 32;
		pm4[i++] = 0xdeadbeaf;
		pm4[i++] = sdma_write_length;

		amdgpu_sdma_test_exec_cs(context_handle, 0,
					i, pm4,
					1, resources,
					ib_info, ibs_request);

		/* verify if SDMA test result meets with expected */
		i = 0;
		while(i < (sdma_write_length / 4)) {
			CU_ASSERT_EQUAL(bo_cpu[i++], 0xdeadbeaf);
		}
		amdgpu_bo_free(bo);
		loop++;
	}
	/* clean resources */
	free(resources);
	free(ibs_request);
	free(ib_info);
	free(pm4);

	/* end of test */
	r = amdgpu_cs_ctx_free(device_handle, context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_command_submission_sdma_copy_linear(void)
{
	const int sdma_write_length = 1024;
	const int pm4_dw = 256;
	amdgpu_context_handle context_handle;
	amdgpu_bo_handle bo1, bo2;
	amdgpu_bo_handle *resources;
	uint32_t *pm4;
	struct amdgpu_cs_ib_info *ib_info;
	struct amdgpu_cs_request *ibs_request;
	uint64_t bo1_mc, bo2_mc;
	volatile unsigned char *bo1_cpu, *bo2_cpu;
	int i, j, r, loop1, loop2;
	uint64_t gtt_flags[3] = {0, AMDGPU_GEM_CREATE_CPU_GTT_UC,
				AMDGPU_GEM_CREATE_CPU_GTT_WC};

	pm4 = calloc(pm4_dw, sizeof(*pm4));
	CU_ASSERT_NOT_EQUAL(pm4, NULL);

	ib_info = calloc(1, sizeof(*ib_info));
	CU_ASSERT_NOT_EQUAL(ib_info, NULL);

	ibs_request = calloc(1, sizeof(*ibs_request));
	CU_ASSERT_NOT_EQUAL(ibs_request, NULL);

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	/* prepare resource */
	resources = calloc(2, sizeof(amdgpu_bo_handle));
	CU_ASSERT_NOT_EQUAL(resources, NULL);

	loop1 = loop2 = 0;
	/* run 9 circle to test all mapping combination */
	while(loop1 < 3) {
		while(loop2 < 3) {
			/* allocate UC bo1for sDMA use */
			bo1 = gpu_mem_alloc(device_handle,
					sdma_write_length, 4096,
					AMDGPU_GEM_DOMAIN_GTT,
					gtt_flags[loop1], &bo1_mc);

			CU_ASSERT_EQUAL(amdgpu_bo_cpu_map(bo1, (void **)&bo1_cpu), 0);
			CU_ASSERT_NOT_EQUAL(bo1_cpu, NULL);

			/* set bo1 */
			memset((void*)bo1_cpu, 0xaa, sdma_write_length);

			/* allocate UC bo2 for sDMA use */
			bo2 = gpu_mem_alloc(device_handle,
					sdma_write_length, 4096,
					AMDGPU_GEM_DOMAIN_GTT,
					gtt_flags[loop2], &bo2_mc);

			CU_ASSERT_EQUAL(amdgpu_bo_cpu_map(bo2, (void **)&bo2_cpu), 0);
			CU_ASSERT_NOT_EQUAL(bo2_cpu, NULL);

			/* clear bo2 */
			memset((void*)bo2_cpu, 0, sdma_write_length);

			resources[0] = bo1;
			resources[1] = bo2;

			/* fullfill PM4: test DMA copy linear */
			i = j = 0;
			pm4[i++] = SDMA_PACKET(SDMA_OPCODE_COPY, SDMA_COPY_SUB_OPCODE_LINEAR, 0);
			pm4[i++] = sdma_write_length;
			pm4[i++] = 0;
			pm4[i++] = 0xffffffff & bo1_mc;
			pm4[i++] = (0xffffffff00000000 & bo1_mc) >> 32;
			pm4[i++] = 0xffffffff & bo2_mc;
			pm4[i++] = (0xffffffff00000000 & bo2_mc) >> 32;


			amdgpu_sdma_test_exec_cs(context_handle, 0,
						i, pm4,
						2, resources,
						ib_info, ibs_request);

			/* verify if SDMA test result meets with expected */
			i = 0;
			while(i < sdma_write_length) {
				CU_ASSERT_EQUAL(bo2_cpu[i++], 0xaa);
			}
			amdgpu_bo_free(bo1);
			amdgpu_bo_free(bo2);
			loop2++;
		}
		loop1++;
	}
	/* clean resources */
	free(resources);
	free(ibs_request);
	free(ib_info);
	free(pm4);

	/* end of test */
	r = amdgpu_cs_ctx_free(device_handle, context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_command_submission_sdma(void)
{
	amdgpu_command_submission_sdma_write_linear();
	amdgpu_command_submission_sdma_const_fill();
	amdgpu_command_submission_sdma_copy_linear();
}

static void amdgpu_userptr_test(void)
{
	int i, r, j;
	uint32_t *pm4 = NULL;
	uint64_t bo_mc;
	void *ptr = NULL;
	int pm4_dw = 256;
	int sdma_write_length = 4;
	amdgpu_bo_handle handle;
	amdgpu_context_handle context_handle;
	struct amdgpu_cs_ib_info *ib_info;
	struct amdgpu_cs_request *ibs_request;
	struct amdgpu_bo_alloc_result res;

	memset(&res, 0, sizeof(res));

	pm4 = calloc(pm4_dw, sizeof(*pm4));
	CU_ASSERT_NOT_EQUAL(pm4, NULL);

	ib_info = calloc(1, sizeof(*ib_info));
	CU_ASSERT_NOT_EQUAL(ib_info, NULL);

	ibs_request = calloc(1, sizeof(*ibs_request));
	CU_ASSERT_NOT_EQUAL(ibs_request, NULL);

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	posix_memalign(&ptr, sysconf(_SC_PAGE_SIZE), BUFFER_SIZE);
	CU_ASSERT_NOT_EQUAL(ptr, NULL);
	memset(ptr, 0, BUFFER_SIZE);

	r = amdgpu_create_bo_from_user_mem(device_handle,
					   ptr, BUFFER_SIZE, &res);
	CU_ASSERT_EQUAL(r, 0);
	bo_mc = res.virtual_mc_base_address;
	handle = res.buf_handle;

	j = i = 0;
	pm4[i++] = SDMA_PACKET(SDMA_OPCODE_WRITE,
			       SDMA_WRITE_SUB_OPCODE_LINEAR, 0);
	pm4[i++] = 0xffffffff & bo_mc;
	pm4[i++] = (0xffffffff00000000 & bo_mc) >> 32;
	pm4[i++] = sdma_write_length;

	while (j++ < sdma_write_length)
		pm4[i++] = 0xdeadbeaf;

	amdgpu_sdma_test_exec_cs(context_handle, 0,
				 i, pm4,
				 1, &handle,
				 ib_info, ibs_request);
	i = 0;
	while (i < sdma_write_length) {
		CU_ASSERT_EQUAL(((int*)ptr)[i++], 0xdeadbeaf);
	}
	free(ibs_request);
	free(ib_info);
	free(pm4);
	r = amdgpu_bo_free(res.buf_handle);
	CU_ASSERT_EQUAL(r, 0);
	free(ptr);

	r = amdgpu_cs_ctx_free(device_handle, context_handle);
	CU_ASSERT_EQUAL(r, 0);
}
