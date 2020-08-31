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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include "CUnit/Basic.h"

#include "amdgpu_test.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"

#include <pthread.h>


/*
 * This defines the delay in MS after which memory location designated for
 * compression against reference value is written to, unblocking command
 * processor
 */
#define WRITE_MEM_ADDRESS_DELAY_MS 100

#define	PACKET_TYPE3	3

#define PACKET3(op, n)	((PACKET_TYPE3 << 30) |				\
			 (((op) & 0xFF) << 8) |				\
			 ((n) & 0x3FFF) << 16)

#define	PACKET3_WAIT_REG_MEM				0x3C
#define		WAIT_REG_MEM_FUNCTION(x)                ((x) << 0)
		/* 0 - always
		 * 1 - <
		 * 2 - <=
		 * 3 - ==
		 * 4 - !=
		 * 5 - >=
		 * 6 - >
		 */
#define		WAIT_REG_MEM_MEM_SPACE(x)               ((x) << 4)
		/* 0 - reg
		 * 1 - mem
		 */
#define		WAIT_REG_MEM_OPERATION(x)               ((x) << 6)
		/* 0 - wait_reg_mem
		 * 1 - wr_wait_wr_reg
		 */
#define		WAIT_REG_MEM_ENGINE(x)                  ((x) << 8)
		/* 0 - me
		 * 1 - pfp
		 */

#define	PACKET3_WRITE_DATA				0x37
#define		WRITE_DATA_DST_SEL(x)                   ((x) << 8)
		/* 0 - register
		 * 1 - memory (sync - via GRBM)
		 * 2 - gl2
		 * 3 - gds
		 * 4 - reserved
		 * 5 - memory (async - direct)
		 */
#define		WR_ONE_ADDR                             (1 << 16)
#define		WR_CONFIRM                              (1 << 20)
#define		WRITE_DATA_CACHE_POLICY(x)              ((x) << 25)
		/* 0 - LRU
		 * 1 - Stream
		 */
#define		WRITE_DATA_ENGINE_SEL(x)                ((x) << 30)
		/* 0 - me
		 * 1 - pfp
		 * 2 - ce
		 */

#define mmVM_CONTEXT0_PAGE_TABLE_BASE_ADDR                                      0x54f

#define SDMA_PKT_HEADER_OP(x)	(x & 0xff)
#define SDMA_OP_POLL_REGMEM  8

static  amdgpu_device_handle device_handle;
static  uint32_t  major_version;
static  uint32_t  minor_version;

static pthread_t stress_thread;
static uint32_t *ptr;

int use_uc_mtype = 0;

static void amdgpu_deadlock_helper(unsigned ip_type);
static void amdgpu_deadlock_gfx(void);
static void amdgpu_deadlock_compute(void);
static void amdgpu_illegal_reg_access();
static void amdgpu_illegal_mem_access();
static void amdgpu_deadlock_sdma(void);

CU_BOOL suite_deadlock_tests_enable(void)
{
	CU_BOOL enable = CU_TRUE;

	if (amdgpu_device_initialize(drm_amdgpu[0], &major_version,
					     &minor_version, &device_handle))
		return CU_FALSE;

	/*
	 * Only enable for ASICs supporting GPU reset and for which it's enabled
	 * by default (currently GFX8/9 dGPUS)
	 */
	if (device_handle->info.family_id != AMDGPU_FAMILY_VI &&
	    device_handle->info.family_id != AMDGPU_FAMILY_AI &&
	    device_handle->info.family_id != AMDGPU_FAMILY_CI) {
		printf("\n\nGPU reset is not enabled for the ASIC, deadlock suite disabled\n");
		enable = CU_FALSE;
	}

	if (device_handle->info.family_id >= AMDGPU_FAMILY_AI)
		use_uc_mtype = 1;

	if (amdgpu_device_deinitialize(device_handle))
		return CU_FALSE;

	return enable;
}

int suite_deadlock_tests_init(void)
{
	int r;

	r = amdgpu_device_initialize(drm_amdgpu[0], &major_version,
				   &minor_version, &device_handle);

	if (r) {
		if ((r == -EACCES) && (errno == EACCES))
			printf("\n\nError:%s. "
				"Hint:Try to run this test program as root.",
				strerror(errno));
		return CUE_SINIT_FAILED;
	}

	return CUE_SUCCESS;
}

int suite_deadlock_tests_clean(void)
{
	int r = amdgpu_device_deinitialize(device_handle);

	if (r == 0)
		return CUE_SUCCESS;
	else
		return CUE_SCLEAN_FAILED;
}


CU_TestInfo deadlock_tests[] = {
	{ "gfx ring block test (set amdgpu.lockup_timeout=50)", amdgpu_deadlock_gfx },
	{ "compute ring block test (set amdgpu.lockup_timeout=50)", amdgpu_deadlock_compute },
	{ "sdma ring block test (set amdgpu.lockup_timeout=50)", amdgpu_deadlock_sdma },
	{ "illegal reg access test", amdgpu_illegal_reg_access },
	{ "illegal mem access test (set amdgpu.vm_fault_stop=2)", amdgpu_illegal_mem_access },
	CU_TEST_INFO_NULL,
};

static void *write_mem_address(void *data)
{
	int i;

	/* useconds_t range is [0, 1,000,000] so use loop for waits > 1s */
	for (i = 0; i < WRITE_MEM_ADDRESS_DELAY_MS; i++)
		usleep(1000);

	ptr[256] = 0x1;

	return 0;
}

static void amdgpu_deadlock_gfx(void)
{
	amdgpu_deadlock_helper(AMDGPU_HW_IP_GFX);
}

static void amdgpu_deadlock_compute(void)
{
	amdgpu_deadlock_helper(AMDGPU_HW_IP_COMPUTE);
}

static void amdgpu_deadlock_helper(unsigned ip_type)
{
	amdgpu_context_handle context_handle;
	amdgpu_bo_handle ib_result_handle;
	void *ib_result_cpu;
	uint64_t ib_result_mc_address;
	struct amdgpu_cs_request ibs_request;
	struct amdgpu_cs_ib_info ib_info;
	struct amdgpu_cs_fence fence_status;
	uint32_t expired;
	int i, r;
	amdgpu_bo_list_handle bo_list;
	amdgpu_va_handle va_handle;

	r = pthread_create(&stress_thread, NULL, write_mem_address, NULL);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_alloc_and_map_raw(device_handle, 4096, 4096,
			AMDGPU_GEM_DOMAIN_GTT, 0, use_uc_mtype ? AMDGPU_VM_MTYPE_UC : 0,
						    &ib_result_handle, &ib_result_cpu,
						    &ib_result_mc_address, &va_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_get_bo_list(device_handle, ib_result_handle, NULL,
			       &bo_list);
	CU_ASSERT_EQUAL(r, 0);

	ptr = ib_result_cpu;

	ptr[0] = PACKET3(PACKET3_WAIT_REG_MEM, 5);
	ptr[1] = (WAIT_REG_MEM_MEM_SPACE(1) | /* memory */
			 WAIT_REG_MEM_FUNCTION(4) | /* != */
			 WAIT_REG_MEM_ENGINE(0));  /* me */
	ptr[2] = (ib_result_mc_address + 256*4) & 0xfffffffc;
	ptr[3] = ((ib_result_mc_address + 256*4) >> 32) & 0xffffffff;
	ptr[4] = 0x00000000; /* reference value */
	ptr[5] = 0xffffffff; /* and mask */
	ptr[6] = 0x00000004; /* poll interval */

	for (i = 7; i < 16; ++i)
		ptr[i] = 0xffff1000;


	ptr[256] = 0x0; /* the memory we wait on to change */



	memset(&ib_info, 0, sizeof(struct amdgpu_cs_ib_info));
	ib_info.ib_mc_address = ib_result_mc_address;
	ib_info.size = 16;

	memset(&ibs_request, 0, sizeof(struct amdgpu_cs_request));
	ibs_request.ip_type = ip_type;
	ibs_request.ring = 0;
	ibs_request.number_of_ibs = 1;
	ibs_request.ibs = &ib_info;
	ibs_request.resources = bo_list;
	ibs_request.fence_info.handle = NULL;
	for (i = 0; i < 200; i++) {
		r = amdgpu_cs_submit(context_handle, 0,&ibs_request, 1);
		CU_ASSERT_EQUAL((r == 0 || r == -ECANCELED), 1);

	}

	memset(&fence_status, 0, sizeof(struct amdgpu_cs_fence));
	fence_status.context = context_handle;
	fence_status.ip_type = ip_type;
	fence_status.ip_instance = 0;
	fence_status.ring = 0;
	fence_status.fence = ibs_request.seq_no;

	r = amdgpu_cs_query_fence_status(&fence_status,
			AMDGPU_TIMEOUT_INFINITE,0, &expired);
	CU_ASSERT_EQUAL((r == 0 || r == -ECANCELED), 1);

	pthread_join(stress_thread, NULL);

	r = amdgpu_bo_list_destroy(bo_list);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_unmap_and_free(ib_result_handle, va_handle,
				     ib_result_mc_address, 4096);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_ctx_free(context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_deadlock_sdma(void)
{
	amdgpu_context_handle context_handle;
	amdgpu_bo_handle ib_result_handle;
	void *ib_result_cpu;
	uint64_t ib_result_mc_address;
	struct amdgpu_cs_request ibs_request;
	struct amdgpu_cs_ib_info ib_info;
	struct amdgpu_cs_fence fence_status;
	uint32_t expired;
	int i, r;
	amdgpu_bo_list_handle bo_list;
	amdgpu_va_handle va_handle;
	struct drm_amdgpu_info_hw_ip info;
	uint32_t ring_id;

	r = amdgpu_query_hw_ip_info(device_handle, AMDGPU_HW_IP_DMA, 0, &info);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	for (ring_id = 0; (1 << ring_id) & info.available_rings; ring_id++) {
		r = pthread_create(&stress_thread, NULL, write_mem_address, NULL);
		CU_ASSERT_EQUAL(r, 0);

		r = amdgpu_bo_alloc_and_map_raw(device_handle, 4096, 4096,
				AMDGPU_GEM_DOMAIN_GTT, 0, use_uc_mtype ? AMDGPU_VM_MTYPE_UC : 0,
							    &ib_result_handle, &ib_result_cpu,
							    &ib_result_mc_address, &va_handle);
		CU_ASSERT_EQUAL(r, 0);

		r = amdgpu_get_bo_list(device_handle, ib_result_handle, NULL,
				       &bo_list);
		CU_ASSERT_EQUAL(r, 0);

		ptr = ib_result_cpu;
		i = 0;

		ptr[i++] = SDMA_PKT_HEADER_OP(SDMA_OP_POLL_REGMEM) |
				(0 << 26) | /* WAIT_REG_MEM */
				(4 << 28) | /* != */
				(1 << 31); /* memory */
		ptr[i++] = (ib_result_mc_address + 256*4) & 0xfffffffc;
		ptr[i++] = ((ib_result_mc_address + 256*4) >> 32) & 0xffffffff;
		ptr[i++] = 0x00000000; /* reference value */
		ptr[i++] = 0xffffffff; /* and mask */
		ptr[i++] =  4 | /* poll interval */
				(0xfff << 16); /* retry count */

		for (; i < 16; i++)
			ptr[i] = 0;

		ptr[256] = 0x0; /* the memory we wait on to change */

		memset(&ib_info, 0, sizeof(struct amdgpu_cs_ib_info));
		ib_info.ib_mc_address = ib_result_mc_address;
		ib_info.size = 16;

		memset(&ibs_request, 0, sizeof(struct amdgpu_cs_request));
		ibs_request.ip_type = AMDGPU_HW_IP_DMA;
		ibs_request.ring = ring_id;
		ibs_request.number_of_ibs = 1;
		ibs_request.ibs = &ib_info;
		ibs_request.resources = bo_list;
		ibs_request.fence_info.handle = NULL;

		for (i = 0; i < 200; i++) {
			r = amdgpu_cs_submit(context_handle, 0,&ibs_request, 1);
			CU_ASSERT_EQUAL((r == 0 || r == -ECANCELED), 1);

		}

		memset(&fence_status, 0, sizeof(struct amdgpu_cs_fence));
		fence_status.context = context_handle;
		fence_status.ip_type = AMDGPU_HW_IP_DMA;
		fence_status.ip_instance = 0;
		fence_status.ring = ring_id;
		fence_status.fence = ibs_request.seq_no;

		r = amdgpu_cs_query_fence_status(&fence_status,
				AMDGPU_TIMEOUT_INFINITE,0, &expired);
		CU_ASSERT_EQUAL((r == 0 || r == -ECANCELED), 1);

		pthread_join(stress_thread, NULL);

		r = amdgpu_bo_list_destroy(bo_list);
		CU_ASSERT_EQUAL(r, 0);

		r = amdgpu_bo_unmap_and_free(ib_result_handle, va_handle,
					     ib_result_mc_address, 4096);
		CU_ASSERT_EQUAL(r, 0);
	}
	r = amdgpu_cs_ctx_free(context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void bad_access_helper(int reg_access)
{
	amdgpu_context_handle context_handle;
	amdgpu_bo_handle ib_result_handle;
	void *ib_result_cpu;
	uint64_t ib_result_mc_address;
	struct amdgpu_cs_request ibs_request;
	struct amdgpu_cs_ib_info ib_info;
	struct amdgpu_cs_fence fence_status;
	uint32_t expired;
	int i, r;
	amdgpu_bo_list_handle bo_list;
	amdgpu_va_handle va_handle;

	r = amdgpu_cs_ctx_create(device_handle, &context_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_alloc_and_map_raw(device_handle, 4096, 4096,
			AMDGPU_GEM_DOMAIN_GTT, 0, 0,
							&ib_result_handle, &ib_result_cpu,
							&ib_result_mc_address, &va_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_get_bo_list(device_handle, ib_result_handle, NULL,
				   &bo_list);
	CU_ASSERT_EQUAL(r, 0);

	ptr = ib_result_cpu;
	i = 0;

	ptr[i++] = PACKET3(PACKET3_WRITE_DATA, 3);
	ptr[i++] = (reg_access ? WRITE_DATA_DST_SEL(0) : WRITE_DATA_DST_SEL(5))| WR_CONFIRM;
	ptr[i++] = reg_access ? mmVM_CONTEXT0_PAGE_TABLE_BASE_ADDR : 0xdeadbee0;
	ptr[i++] = 0;
	ptr[i++] = 0xdeadbeef;

	for (; i < 16; ++i)
		ptr[i] = 0xffff1000;

	memset(&ib_info, 0, sizeof(struct amdgpu_cs_ib_info));
	ib_info.ib_mc_address = ib_result_mc_address;
	ib_info.size = 16;

	memset(&ibs_request, 0, sizeof(struct amdgpu_cs_request));
	ibs_request.ip_type = AMDGPU_HW_IP_GFX;
	ibs_request.ring = 0;
	ibs_request.number_of_ibs = 1;
	ibs_request.ibs = &ib_info;
	ibs_request.resources = bo_list;
	ibs_request.fence_info.handle = NULL;

	r = amdgpu_cs_submit(context_handle, 0,&ibs_request, 1);
	CU_ASSERT_EQUAL((r == 0 || r == -ECANCELED), 1);


	memset(&fence_status, 0, sizeof(struct amdgpu_cs_fence));
	fence_status.context = context_handle;
	fence_status.ip_type = AMDGPU_HW_IP_GFX;
	fence_status.ip_instance = 0;
	fence_status.ring = 0;
	fence_status.fence = ibs_request.seq_no;

	r = amdgpu_cs_query_fence_status(&fence_status,
			AMDGPU_TIMEOUT_INFINITE,0, &expired);
	CU_ASSERT_EQUAL((r == 0 || r == -ECANCELED), 1);

	r = amdgpu_bo_list_destroy(bo_list);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_unmap_and_free(ib_result_handle, va_handle,
					 ib_result_mc_address, 4096);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_cs_ctx_free(context_handle);
	CU_ASSERT_EQUAL(r, 0);
}

static void amdgpu_illegal_reg_access()
{
	bad_access_helper(1);
}

static void amdgpu_illegal_mem_access()
{
	bad_access_helper(0);
}
