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

#include "CUnit/Basic.h"

#include "amdgpu_test.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "xf86drm.h"
#include "json.h"

const char *ras_block_string[] = {
	"umc",
	"sdma",
	"gfx",
	"mmhub",
	"athub",
	"pcie_bif",
	"hdp",
	"xgmi_wafl",
	"df",
	"smn",
	"sem",
	"mp0",
	"mp1",
	"fuse",
};

#define ras_block_str(i) (ras_block_string[i])

enum amdgpu_ras_block {
	AMDGPU_RAS_BLOCK__UMC = 0,
	AMDGPU_RAS_BLOCK__SDMA,
	AMDGPU_RAS_BLOCK__GFX,
	AMDGPU_RAS_BLOCK__MMHUB,
	AMDGPU_RAS_BLOCK__ATHUB,
	AMDGPU_RAS_BLOCK__PCIE_BIF,
	AMDGPU_RAS_BLOCK__HDP,
	AMDGPU_RAS_BLOCK__XGMI_WAFL,
	AMDGPU_RAS_BLOCK__DF,
	AMDGPU_RAS_BLOCK__SMN,
	AMDGPU_RAS_BLOCK__SEM,
	AMDGPU_RAS_BLOCK__MP0,
	AMDGPU_RAS_BLOCK__MP1,
	AMDGPU_RAS_BLOCK__FUSE,

	AMDGPU_RAS_BLOCK__LAST
};

#define AMDGPU_RAS_BLOCK_COUNT  AMDGPU_RAS_BLOCK__LAST
#define AMDGPU_RAS_BLOCK_MASK   ((1ULL << AMDGPU_RAS_BLOCK_COUNT) - 1)

enum amdgpu_ras_gfx_subblock {
	/* CPC */
	AMDGPU_RAS_BLOCK__GFX_CPC_INDEX_START = 0,
	AMDGPU_RAS_BLOCK__GFX_CPC_SCRATCH =
		AMDGPU_RAS_BLOCK__GFX_CPC_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_CPC_UCODE,
	AMDGPU_RAS_BLOCK__GFX_DC_STATE_ME1,
	AMDGPU_RAS_BLOCK__GFX_DC_CSINVOC_ME1,
	AMDGPU_RAS_BLOCK__GFX_DC_RESTORE_ME1,
	AMDGPU_RAS_BLOCK__GFX_DC_STATE_ME2,
	AMDGPU_RAS_BLOCK__GFX_DC_CSINVOC_ME2,
	AMDGPU_RAS_BLOCK__GFX_DC_RESTORE_ME2,
	AMDGPU_RAS_BLOCK__GFX_CPC_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_DC_RESTORE_ME2,
	/* CPF */
	AMDGPU_RAS_BLOCK__GFX_CPF_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_CPF_ROQ_ME2 =
		AMDGPU_RAS_BLOCK__GFX_CPF_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_CPF_ROQ_ME1,
	AMDGPU_RAS_BLOCK__GFX_CPF_TAG,
	AMDGPU_RAS_BLOCK__GFX_CPF_INDEX_END = AMDGPU_RAS_BLOCK__GFX_CPF_TAG,
	/* CPG */
	AMDGPU_RAS_BLOCK__GFX_CPG_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_CPG_DMA_ROQ =
		AMDGPU_RAS_BLOCK__GFX_CPG_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_CPG_DMA_TAG,
	AMDGPU_RAS_BLOCK__GFX_CPG_TAG,
	AMDGPU_RAS_BLOCK__GFX_CPG_INDEX_END = AMDGPU_RAS_BLOCK__GFX_CPG_TAG,
	/* GDS */
	AMDGPU_RAS_BLOCK__GFX_GDS_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_GDS_MEM = AMDGPU_RAS_BLOCK__GFX_GDS_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_GDS_INPUT_QUEUE,
	AMDGPU_RAS_BLOCK__GFX_GDS_OA_PHY_CMD_RAM_MEM,
	AMDGPU_RAS_BLOCK__GFX_GDS_OA_PHY_DATA_RAM_MEM,
	AMDGPU_RAS_BLOCK__GFX_GDS_OA_PIPE_MEM,
	AMDGPU_RAS_BLOCK__GFX_GDS_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_GDS_OA_PIPE_MEM,
	/* SPI */
	AMDGPU_RAS_BLOCK__GFX_SPI_SR_MEM,
	/* SQ */
	AMDGPU_RAS_BLOCK__GFX_SQ_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_SQ_SGPR = AMDGPU_RAS_BLOCK__GFX_SQ_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_SQ_LDS_D,
	AMDGPU_RAS_BLOCK__GFX_SQ_LDS_I,
	AMDGPU_RAS_BLOCK__GFX_SQ_VGPR,
	AMDGPU_RAS_BLOCK__GFX_SQ_INDEX_END = AMDGPU_RAS_BLOCK__GFX_SQ_VGPR,
	/* SQC (3 ranges) */
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX_START,
	/* SQC range 0 */
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX0_START =
		AMDGPU_RAS_BLOCK__GFX_SQC_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_UTCL1_LFIFO =
		AMDGPU_RAS_BLOCK__GFX_SQC_INDEX0_START,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU0_WRITE_DATA_BUF,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU0_UTCL1_LFIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU1_WRITE_DATA_BUF,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU1_UTCL1_LFIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU2_WRITE_DATA_BUF,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU2_UTCL1_LFIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX0_END =
		AMDGPU_RAS_BLOCK__GFX_SQC_DATA_CU2_UTCL1_LFIFO,
	/* SQC range 1 */
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX1_START,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKA_TAG_RAM =
		AMDGPU_RAS_BLOCK__GFX_SQC_INDEX1_START,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKA_UTCL1_MISS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKA_MISS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKA_BANK_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKA_TAG_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKA_HIT_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKA_MISS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKA_DIRTY_BIT_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKA_BANK_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX1_END =
		AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKA_BANK_RAM,
	/* SQC range 2 */
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX2_START,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKB_TAG_RAM =
		AMDGPU_RAS_BLOCK__GFX_SQC_INDEX2_START,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKB_UTCL1_MISS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKB_MISS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_INST_BANKB_BANK_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKB_TAG_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKB_HIT_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKB_MISS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKB_DIRTY_BIT_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKB_BANK_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX2_END =
		AMDGPU_RAS_BLOCK__GFX_SQC_DATA_BANKB_BANK_RAM,
	AMDGPU_RAS_BLOCK__GFX_SQC_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_SQC_INDEX2_END,
	/* TA */
	AMDGPU_RAS_BLOCK__GFX_TA_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TA_FS_DFIFO =
		AMDGPU_RAS_BLOCK__GFX_TA_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TA_FS_AFIFO,
	AMDGPU_RAS_BLOCK__GFX_TA_FL_LFIFO,
	AMDGPU_RAS_BLOCK__GFX_TA_FX_LFIFO,
	AMDGPU_RAS_BLOCK__GFX_TA_FS_CFIFO,
	AMDGPU_RAS_BLOCK__GFX_TA_INDEX_END = AMDGPU_RAS_BLOCK__GFX_TA_FS_CFIFO,
	/* TCA */
	AMDGPU_RAS_BLOCK__GFX_TCA_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TCA_HOLE_FIFO =
		AMDGPU_RAS_BLOCK__GFX_TCA_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TCA_REQ_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TCA_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_TCA_REQ_FIFO,
	/* TCC (5 sub-ranges) */
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX_START,
	/* TCC range 0 */
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX0_START =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_DATA =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX0_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_DATA_BANK_0_1,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_DATA_BANK_1_0,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_DATA_BANK_1_1,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_DIRTY_BANK_0,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_DIRTY_BANK_1,
	AMDGPU_RAS_BLOCK__GFX_TCC_HIGH_RATE_TAG,
	AMDGPU_RAS_BLOCK__GFX_TCC_LOW_RATE_TAG,
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX0_END =
		AMDGPU_RAS_BLOCK__GFX_TCC_LOW_RATE_TAG,
	/* TCC range 1 */
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX1_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_IN_USE_DEC =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX1_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_IN_USE_TRANSFER,
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX1_END =
		AMDGPU_RAS_BLOCK__GFX_TCC_IN_USE_TRANSFER,
	/* TCC range 2 */
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX2_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_RETURN_DATA =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX2_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_RETURN_CONTROL,
	AMDGPU_RAS_BLOCK__GFX_TCC_UC_ATOMIC_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TCC_WRITE_RETURN,
	AMDGPU_RAS_BLOCK__GFX_TCC_WRITE_CACHE_READ,
	AMDGPU_RAS_BLOCK__GFX_TCC_SRC_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TCC_SRC_FIFO_NEXT_RAM,
	AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_TAG_PROBE_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX2_END =
		AMDGPU_RAS_BLOCK__GFX_TCC_CACHE_TAG_PROBE_FIFO,
	/* TCC range 3 */
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX3_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_LATENCY_FIFO =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX3_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_LATENCY_FIFO_NEXT_RAM,
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX3_END =
		AMDGPU_RAS_BLOCK__GFX_TCC_LATENCY_FIFO_NEXT_RAM,
	/* TCC range 4 */
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX4_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_WRRET_TAG_WRITE_RETURN =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX4_START,
	AMDGPU_RAS_BLOCK__GFX_TCC_ATOMIC_RETURN_BUFFER,
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX4_END =
		AMDGPU_RAS_BLOCK__GFX_TCC_ATOMIC_RETURN_BUFFER,
	AMDGPU_RAS_BLOCK__GFX_TCC_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_TCC_INDEX4_END,
	/* TCI */
	AMDGPU_RAS_BLOCK__GFX_TCI_WRITE_RAM,
	/* TCP */
	AMDGPU_RAS_BLOCK__GFX_TCP_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TCP_CACHE_RAM =
		AMDGPU_RAS_BLOCK__GFX_TCP_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TCP_LFIFO_RAM,
	AMDGPU_RAS_BLOCK__GFX_TCP_CMD_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TCP_VM_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TCP_DB_RAM,
	AMDGPU_RAS_BLOCK__GFX_TCP_UTCL1_LFIFO0,
	AMDGPU_RAS_BLOCK__GFX_TCP_UTCL1_LFIFO1,
	AMDGPU_RAS_BLOCK__GFX_TCP_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_TCP_UTCL1_LFIFO1,
	/* TD */
	AMDGPU_RAS_BLOCK__GFX_TD_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TD_SS_FIFO_LO =
		AMDGPU_RAS_BLOCK__GFX_TD_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_TD_SS_FIFO_HI,
	AMDGPU_RAS_BLOCK__GFX_TD_CS_FIFO,
	AMDGPU_RAS_BLOCK__GFX_TD_INDEX_END = AMDGPU_RAS_BLOCK__GFX_TD_CS_FIFO,
	/* EA (3 sub-ranges) */
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX_START,
	/* EA range 0 */
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX0_START =
		AMDGPU_RAS_BLOCK__GFX_EA_INDEX_START,
	AMDGPU_RAS_BLOCK__GFX_EA_DRAMRD_CMDMEM =
		AMDGPU_RAS_BLOCK__GFX_EA_INDEX0_START,
	AMDGPU_RAS_BLOCK__GFX_EA_DRAMWR_CMDMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_DRAMWR_DATAMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_RRET_TAGMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_WRET_TAGMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_GMIRD_CMDMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_GMIWR_CMDMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_GMIWR_DATAMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX0_END =
		AMDGPU_RAS_BLOCK__GFX_EA_GMIWR_DATAMEM,
	/* EA range 1 */
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX1_START,
	AMDGPU_RAS_BLOCK__GFX_EA_DRAMRD_PAGEMEM =
		AMDGPU_RAS_BLOCK__GFX_EA_INDEX1_START,
	AMDGPU_RAS_BLOCK__GFX_EA_DRAMWR_PAGEMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_IORD_CMDMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_IOWR_CMDMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_IOWR_DATAMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_GMIRD_PAGEMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_GMIWR_PAGEMEM,
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX1_END =
		AMDGPU_RAS_BLOCK__GFX_EA_GMIWR_PAGEMEM,
	/* EA range 2 */
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX2_START,
	AMDGPU_RAS_BLOCK__GFX_EA_MAM_D0MEM =
		AMDGPU_RAS_BLOCK__GFX_EA_INDEX2_START,
	AMDGPU_RAS_BLOCK__GFX_EA_MAM_D1MEM,
	AMDGPU_RAS_BLOCK__GFX_EA_MAM_D2MEM,
	AMDGPU_RAS_BLOCK__GFX_EA_MAM_D3MEM,
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX2_END =
		AMDGPU_RAS_BLOCK__GFX_EA_MAM_D3MEM,
	AMDGPU_RAS_BLOCK__GFX_EA_INDEX_END =
		AMDGPU_RAS_BLOCK__GFX_EA_INDEX2_END,
	/* UTC VM L2 bank */
	AMDGPU_RAS_BLOCK__UTC_VML2_BANK_CACHE,
	/* UTC VM walker */
	AMDGPU_RAS_BLOCK__UTC_VML2_WALKER,
	/* UTC ATC L2 2MB cache */
	AMDGPU_RAS_BLOCK__UTC_ATCL2_CACHE_2M_BANK,
	/* UTC ATC L2 4KB cache */
	AMDGPU_RAS_BLOCK__UTC_ATCL2_CACHE_4K_BANK,
	AMDGPU_RAS_BLOCK__GFX_MAX
};

enum amdgpu_ras_error_type {
	AMDGPU_RAS_ERROR__NONE					= 0,
	AMDGPU_RAS_ERROR__PARITY				= 1,
	AMDGPU_RAS_ERROR__SINGLE_CORRECTABLE			= 2,
	AMDGPU_RAS_ERROR__MULTI_UNCORRECTABLE			= 4,
	AMDGPU_RAS_ERROR__POISON				= 8,
};

struct ras_test_item {
	char name[64];
	int block;
	int sub_block;
	char error_type_str[64];
	enum amdgpu_ras_error_type type;
	uint64_t address;
	uint64_t value;
};

struct ras_common_if {
	enum amdgpu_ras_block block;
	enum amdgpu_ras_error_type type;
	uint32_t sub_block_index;
	char name[32];
};

struct ras_inject_if {
	struct ras_common_if head;
	uint64_t address;
	uint64_t value;
};

struct ras_debug_if {
	union {
		struct ras_common_if head;
		struct ras_inject_if inject;
	};
	int op;
};
/* for now, only umc, gfx, sdma has implemented. */
#define DEFAULT_RAS_BLOCK_MASK_INJECT ((1 << AMDGPU_RAS_BLOCK__UMC) |\
		(1 << AMDGPU_RAS_BLOCK__GFX))
#define DEFAULT_RAS_BLOCK_MASK_QUERY ((1 << AMDGPU_RAS_BLOCK__UMC) |\
		(1 << AMDGPU_RAS_BLOCK__GFX))
#define DEFAULT_RAS_BLOCK_MASK_BASIC (1 << AMDGPU_RAS_BLOCK__UMC |\
		(1 << AMDGPU_RAS_BLOCK__SDMA) |\
		(1 << AMDGPU_RAS_BLOCK__GFX))

static uint32_t ras_block_mask_inject = DEFAULT_RAS_BLOCK_MASK_INJECT;
static uint32_t ras_block_mask_query = DEFAULT_RAS_BLOCK_MASK_INJECT;
static uint32_t ras_block_mask_basic = DEFAULT_RAS_BLOCK_MASK_BASIC;

struct ras_test_mask {
	uint32_t inject_mask;
	uint32_t query_mask;
	uint32_t basic_mask;
};

struct amdgpu_ras_data {
	amdgpu_device_handle device_handle;
	uint32_t  id;
	uint32_t  capability;
	struct ras_test_mask test_mask;
};

/* all devices who has ras supported */
static struct amdgpu_ras_data devices[MAX_CARDS_SUPPORTED];
static int devices_count;

struct ras_DID_test_mask{
	uint16_t device_id;
	uint16_t revision_id;
	struct ras_test_mask test_mask;
};

/* white list for inject test. */
#define RAS_BLOCK_MASK_ALL {\
	DEFAULT_RAS_BLOCK_MASK_INJECT,\
	DEFAULT_RAS_BLOCK_MASK_QUERY,\
	DEFAULT_RAS_BLOCK_MASK_BASIC\
}

#define RAS_BLOCK_MASK_QUERY_BASIC {\
	0,\
	DEFAULT_RAS_BLOCK_MASK_QUERY,\
	DEFAULT_RAS_BLOCK_MASK_BASIC\
}

static const struct ras_DID_test_mask ras_DID_array[] = {
	{0x66a1, 0x00, RAS_BLOCK_MASK_ALL},
	{0x66a1, 0x01, RAS_BLOCK_MASK_ALL},
	{0x66a1, 0x04, RAS_BLOCK_MASK_ALL},
};

static struct ras_test_mask amdgpu_ras_get_test_mask(drmDevicePtr device)
{
	int i;
	static struct ras_test_mask default_test_mask = RAS_BLOCK_MASK_QUERY_BASIC;

	for (i = 0; i < sizeof(ras_DID_array) / sizeof(ras_DID_array[0]); i++) {
		if (ras_DID_array[i].device_id == device->deviceinfo.pci->device_id &&
				ras_DID_array[i].revision_id == device->deviceinfo.pci->revision_id)
			return ras_DID_array[i].test_mask;
	}
	return default_test_mask;
}

static uint32_t amdgpu_ras_lookup_capability(amdgpu_device_handle device_handle)
{
	union {
		uint64_t feature_mask;
		struct {
			uint32_t enabled_features;
			uint32_t supported_features;
		};
	} features = { 0 };
	int ret;

	ret = amdgpu_query_info(device_handle, AMDGPU_INFO_RAS_ENABLED_FEATURES,
			sizeof(features), &features);
	if (ret)
		return 0;

	return features.supported_features;
}

static int get_file_contents(char *file, char *buf, int size);

static int amdgpu_ras_lookup_id(drmDevicePtr device)
{
	char path[1024];
	char str[128];
	drmPciBusInfo info;
	int i;
	int ret;

	for (i = 0; i < MAX_CARDS_SUPPORTED; i++) {
		memset(str, 0, sizeof(str));
		memset(&info, 0, sizeof(info));
		sprintf(path, "/sys/kernel/debug/dri/%d/name", i);
		if (get_file_contents(path, str, sizeof(str)) <= 0)
			continue;

		ret = sscanf(str, "amdgpu dev=%04hx:%02hhx:%02hhx.%01hhx",
				&info.domain, &info.bus, &info.dev, &info.func);
		if (ret != 4)
			continue;

		if (memcmp(&info, device->businfo.pci, sizeof(info)) == 0)
				return i;
	}
	return -1;
}

CU_BOOL suite_ras_tests_enable(void)
{
	amdgpu_device_handle device_handle;
	uint32_t  major_version;
	uint32_t  minor_version;
	int i;
	drmDevicePtr device;

	for (i = 0; i < MAX_CARDS_SUPPORTED && drm_amdgpu[i] >= 0; i++) {
		if (amdgpu_device_initialize(drm_amdgpu[i], &major_version,
					&minor_version, &device_handle))
			continue;

		if (drmGetDevice2(drm_amdgpu[i],
					DRM_DEVICE_GET_PCI_REVISION,
					&device))
			continue;

		if (device->bustype == DRM_BUS_PCI &&
				amdgpu_ras_lookup_capability(device_handle)) {
			amdgpu_device_deinitialize(device_handle);
			return CU_TRUE;
		}

		if (amdgpu_device_deinitialize(device_handle))
			continue;
	}

	return CU_FALSE;
}

int suite_ras_tests_init(void)
{
	drmDevicePtr device;
	amdgpu_device_handle device_handle;
	uint32_t  major_version;
	uint32_t  minor_version;
	uint32_t  capability;
	struct ras_test_mask test_mask;
	int id;
	int i;
	int r;

	for (i = 0; i < MAX_CARDS_SUPPORTED && drm_amdgpu[i] >= 0; i++) {
		r = amdgpu_device_initialize(drm_amdgpu[i], &major_version,
				&minor_version, &device_handle);
		if (r)
			continue;

		if (drmGetDevice2(drm_amdgpu[i],
					DRM_DEVICE_GET_PCI_REVISION,
					&device)) {
			amdgpu_device_deinitialize(device_handle);
			continue;
		}

		if (device->bustype != DRM_BUS_PCI) {
			amdgpu_device_deinitialize(device_handle);
			continue;
		}

		capability = amdgpu_ras_lookup_capability(device_handle);
		if (capability == 0) {
			amdgpu_device_deinitialize(device_handle);
			continue;

		}

		id = amdgpu_ras_lookup_id(device);
		if (id == -1) {
			amdgpu_device_deinitialize(device_handle);
			continue;
		}

		test_mask = amdgpu_ras_get_test_mask(device);

		devices[devices_count++] = (struct amdgpu_ras_data) {
			device_handle, id, capability, test_mask,
		};
	}

	if (devices_count == 0)
		return CUE_SINIT_FAILED;

	return CUE_SUCCESS;
}

int suite_ras_tests_clean(void)
{
	int r;
	int i;
	int ret = CUE_SUCCESS;

	for (i = 0; i < devices_count; i++) {
		r = amdgpu_device_deinitialize(devices[i].device_handle);
		if (r)
			ret = CUE_SCLEAN_FAILED;
	}
	return ret;
}

static void amdgpu_ras_disable_test(void);
static void amdgpu_ras_enable_test(void);
static void amdgpu_ras_inject_test(void);
static void amdgpu_ras_query_test(void);
static void amdgpu_ras_basic_test(void);

CU_TestInfo ras_tests[] = {
	{ "ras basic test",	amdgpu_ras_basic_test },
	{ "ras query test",	amdgpu_ras_query_test },
	{ "ras inject test",	amdgpu_ras_inject_test },
	{ "ras disable test",	amdgpu_ras_disable_test },
#if 0
	{ "ras enable test",	amdgpu_ras_enable_test },
#endif
	CU_TEST_INFO_NULL,
};

//helpers

static int test_card;
static char sysfs_path[1024];
static char debugfs_path[1024];
static uint32_t ras_mask;
static amdgpu_device_handle device_handle;

static int set_test_card(int card)
{
	int i;

	test_card = card;
	sprintf(sysfs_path, "/sys/class/drm/card%d/device/ras/", devices[card].id);
	sprintf(debugfs_path, "/sys/kernel/debug/dri/%d/ras/", devices[card].id);
	ras_mask = devices[card].capability;
	device_handle = devices[card].device_handle;
	ras_block_mask_inject = devices[card].test_mask.inject_mask;
	ras_block_mask_query = devices[card].test_mask.query_mask;
	ras_block_mask_basic = devices[card].test_mask.basic_mask;

	return 0;
}

static const char *get_ras_sysfs_root(void)
{
	return sysfs_path;
}

static const char *get_ras_debugfs_root(void)
{
	return debugfs_path;
}

static int set_file_contents(char *file, char *buf, int size)
{
	int n, fd;
	fd = open(file, O_WRONLY);
	if (fd == -1)
		return -1;
	n = write(fd, buf, size);
	close(fd);
	return n;
}

static int get_file_contents(char *file, char *buf, int size)
{
	int n, fd;
	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;
	n = read(fd, buf, size);
	close(fd);
	return n;
}

static int is_file_ok(char *file, int flags)
{
	int fd;

	fd = open(file, flags);
	if (fd == -1)
		return -1;
	close(fd);
	return 0;
}

static int amdgpu_ras_is_feature_enabled(enum amdgpu_ras_block block)
{
	uint32_t feature_mask;
	int ret;

	ret = amdgpu_query_info(device_handle, AMDGPU_INFO_RAS_ENABLED_FEATURES,
			sizeof(feature_mask), &feature_mask);
	if (ret)
		return -1;

	return (1 << block) & feature_mask;
}

static int amdgpu_ras_is_feature_supported(enum amdgpu_ras_block block)
{
	return (1 << block) & ras_mask;
}

static int amdgpu_ras_invoke(struct ras_debug_if *data)
{
	char path[1024];
	int ret;

	sprintf(path, "%s%s", get_ras_debugfs_root(), "ras_ctrl");

	ret = set_file_contents(path, (char *)data, sizeof(*data))
		- sizeof(*data);
	return ret;
}

static int amdgpu_ras_query_err_count(enum amdgpu_ras_block block,
		unsigned long *ue, unsigned long *ce)
{
	char buf[64];
	char name[1024];
	int ret;

	*ue = *ce = 0;

	if (amdgpu_ras_is_feature_supported(block) <= 0)
		return -1;

	sprintf(name, "%s%s%s", get_ras_sysfs_root(), ras_block_str(block), "_err_count");

	if (is_file_ok(name, O_RDONLY))
		return 0;

	if (get_file_contents(name, buf, sizeof(buf)) <= 0)
		return -1;

	if (sscanf(buf, "ue: %lu\nce: %lu", ue, ce) != 2)
		return -1;

	return 0;
}

static int amdgpu_ras_inject(enum amdgpu_ras_block block,
		uint32_t sub_block, enum amdgpu_ras_error_type type,
		uint64_t address, uint64_t value)
{
	struct ras_debug_if data = { .op = 2, };
	struct ras_inject_if *inject = &data.inject;
	int ret;

	if (amdgpu_ras_is_feature_enabled(block) <= 0) {
		fprintf(stderr, "block id(%d) is not valid\n", block);
		return -1;
	}

	inject->head.block = block;
	inject->head.type = type;
	inject->head.sub_block_index = sub_block;
	strncpy(inject->head.name, ras_block_str(block), 32);
	inject->address = address;
	inject->value = value;

	ret = amdgpu_ras_invoke(&data);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret)
		return -1;

	return 0;
}

//tests
static void amdgpu_ras_features_test(int enable)
{
	struct ras_debug_if data;
	int ret;
	int i;

	data.op = enable;
	for (i = 0; i < AMDGPU_RAS_BLOCK__LAST; i++) {
		struct ras_common_if head = {
			.block = i,
			.type = AMDGPU_RAS_ERROR__MULTI_UNCORRECTABLE,
			.sub_block_index = 0,
			.name = "",
		};

		if (amdgpu_ras_is_feature_supported(i) <= 0)
			continue;

		data.head = head;

		ret = amdgpu_ras_invoke(&data);
		CU_ASSERT_EQUAL(ret, 0);

		if (ret)
			continue;

		ret = enable ^ amdgpu_ras_is_feature_enabled(i);
		CU_ASSERT_EQUAL(ret, 0);
	}
}

static void amdgpu_ras_disable_test(void)
{
	int i;
	for (i = 0; i < devices_count; i++) {
		set_test_card(i);
		amdgpu_ras_features_test(0);
	}
}

static void amdgpu_ras_enable_test(void)
{
	int i;
	for (i = 0; i < devices_count; i++) {
		set_test_card(i);
		amdgpu_ras_features_test(1);
	}
}

static int _json_get_block_id(json_object *block_obj, const char *name)
{
	json_object *item_obj, *index_obj;

	if (!json_object_object_get_ex(block_obj, name, &item_obj))
		return -1;

	if (!json_object_object_get_ex(item_obj, "index", &index_obj))
		return -1;

	return json_object_get_int(index_obj);
}

static int _json_get_subblock_id(json_object *block_obj, const char *block_name,
				 const char *subblock_name)
{
	json_object *item_obj, *subblock_obj, *name_obj;

	if (!json_object_object_get_ex(block_obj, block_name, &item_obj))
		return -1;

	if (!json_object_object_get_ex(item_obj, "subblock", &subblock_obj))
		return -1;

	if (!json_object_object_get_ex(subblock_obj, subblock_name, &name_obj))
		return -1;

	return json_object_get_int(name_obj);
}

static int amdgpu_ras_get_test_items(struct ras_test_item **pitems, int *size)
{
	json_object *root_obj = NULL;
	json_object *block_obj = NULL;
	json_object *type_obj = NULL;
	json_object *tests_obj = NULL;
	json_object *test_obj = NULL;
	json_object *tmp_obj = NULL;
	json_object *tmp_type_obj = NULL;
	json_object *subblock_obj = NULL;
	int i, length;
	struct ras_test_item *items = NULL;
	int ret = -1;

	root_obj = json_object_from_file("./amdgpu_ras.json");
	if (!root_obj)
		root_obj = json_object_from_file(
			"/usr/share/libdrm/amdgpu_ras.json");

	if (!root_obj) {
		CU_FAIL_FATAL("Couldn't find amdgpu_ras.json");
		goto pro_end;
	}

	/* Check Version */
	if (!json_object_object_get_ex(root_obj, "version", &tmp_obj)) {
		CU_FAIL_FATAL("Wrong format of amdgpu_ras.json");
		goto pro_end;
	}

	/* Block Definition */
	if (!json_object_object_get_ex(root_obj, "block", &block_obj)) {
		fprintf(stderr, "block isn't defined\n");
		goto pro_end;
	}

	/* Type Definition */
	if (!json_object_object_get_ex(root_obj, "type", &type_obj)) {
		fprintf(stderr, "type isn't defined\n");
		goto pro_end;
	}

	/* Enumulate test items */
	if (!json_object_object_get_ex(root_obj, "tests", &tests_obj)) {
		fprintf(stderr, "tests are empty\n");
		goto pro_end;
	}

	length = json_object_array_length(tests_obj);

	items = malloc(sizeof(struct ras_test_item) * length);
	if (!items) {
		fprintf(stderr, "malloc failed\n");
		goto pro_end;
	}

	for (i = 0; i < length; i++) {
		test_obj = json_object_array_get_idx(tests_obj, i);

		/* Name */
		if (!json_object_object_get_ex(test_obj, "name", &tmp_obj)) {
			fprintf(stderr, "Test %d has no name\n", i);
			goto pro_end;
		}
		strncpy(items[i].name, json_object_get_string(tmp_obj), 64);

		/* block */
		if (!json_object_object_get_ex(test_obj, "block", &tmp_obj)) {
			fprintf(stderr, "Test:%s: block isn't defined\n",
				items[i].name);
			goto pro_end;
		}
		items[i].block = _json_get_block_id(
			block_obj, json_object_get_string(tmp_obj));

		/* check block id */
		if (items[i].block < AMDGPU_RAS_BLOCK__UMC ||
		    items[i].block >= AMDGPU_RAS_BLOCK__LAST) {
			fprintf(stderr, "Test:%s: block id %d is invalid\n",
				items[i].name, items[i].block);
			goto pro_end;
		}

		/* subblock */
		if (json_object_object_get_ex(test_obj, "subblock", &tmp_obj)) {
			json_object_object_get_ex(test_obj, "block",
				&subblock_obj);

			items[i].sub_block = _json_get_subblock_id(
				block_obj,
				json_object_get_string(subblock_obj),
				json_object_get_string(tmp_obj));
			if (items[i].sub_block < 0) {
				fprintf(stderr, "Test:%s: subblock in block id %d is invalid\n",
					items[i].name, items[i].block);
				goto pro_end;
			}
		} else
			items[i].sub_block = 0;

		/* type */
		if (json_object_object_get_ex(test_obj, "type", &tmp_obj)) {
			strncpy(items[i].error_type_str,
				json_object_get_string(tmp_obj), 64);

			if (json_object_object_get_ex(type_obj,
				json_object_get_string(tmp_obj), &tmp_type_obj))
				items[i].type = json_object_get_int(tmp_type_obj);
			else
				items[i].type = (enum amdgpu_ras_error_type)0;
		}

		/* address */
		if (json_object_object_get_ex(test_obj, "address", &tmp_obj))
			items[i].address = json_object_get_int(tmp_obj);
		else
			items[i].address = 0; /* default address 0 */

		/* value */
		if (json_object_object_get_ex(test_obj, "value", &tmp_obj))
			items[i].value = json_object_get_int(tmp_obj);
		else
			items[i].value = 0; /* default value 0 */
	}

	*pitems = items;
	*size = length;
	ret = 0;
pro_end:
	if (root_obj)
		json_object_put(root_obj);

	return ret;
}

static void __amdgpu_ras_inject_test(void)
{
	struct ras_test_item *items = NULL;
	int i, size;
	int ret;
	unsigned long old_ue, old_ce;
	unsigned long ue, ce;
	int timeout;
	bool pass;

	ret = amdgpu_ras_get_test_items(&items, &size);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret)
		goto mem_free;

	printf("...\n");
	for (i = 0; i < size; i++) {
		timeout = 3;
		pass = false;

		ret = amdgpu_ras_query_err_count(items[i].block, &old_ue,
						 &old_ce);
		CU_ASSERT_EQUAL(ret, 0);
		if (ret)
			break;

		ret = amdgpu_ras_inject(items[i].block, items[i].sub_block,
					items[i].type, items[i].address,
					items[i].value);
		CU_ASSERT_EQUAL(ret, 0);
		if (ret)
			break;

		while (timeout > 0) {
			sleep(5);

			ret = amdgpu_ras_query_err_count(items[i].block, &ue,
							 &ce);
			CU_ASSERT_EQUAL(ret, 0);
			if (ret)
				break;

			if (old_ue != ue || old_ce != ce) {
				pass = true;
				sleep(20);
				break;
			}
			timeout -= 1;
		}
		printf("\t Test %s@%s, address %ld, value %ld: %s\n",
			items[i].name, items[i].error_type_str, items[i].address,
			items[i].value,	pass ? "Pass" : "Fail");
	}

mem_free:
	if (items) {
		free(items);
		items = NULL;
	}
}

static void amdgpu_ras_inject_test(void)
{
	int i;
	for (i = 0; i < devices_count; i++) {
		set_test_card(i);
		__amdgpu_ras_inject_test();
	}
}

static void __amdgpu_ras_query_test(void)
{
	unsigned long ue, ce;
	int ret;
	int i;

	for (i = 0; i < AMDGPU_RAS_BLOCK__LAST; i++) {
		if (amdgpu_ras_is_feature_supported(i) <= 0)
			continue;

		if (!((1 << i) & ras_block_mask_query))
			continue;

		ret = amdgpu_ras_query_err_count(i, &ue, &ce);
		CU_ASSERT_EQUAL(ret, 0);
	}
}

static void amdgpu_ras_query_test(void)
{
	int i;
	for (i = 0; i < devices_count; i++) {
		set_test_card(i);
		__amdgpu_ras_query_test();
	}
}

static void amdgpu_ras_basic_test(void)
{
	unsigned long ue, ce;
	char name[1024];
	int ret;
	int i;
	int j;
	uint32_t features;
	char path[1024];

	ret = is_file_ok("/sys/module/amdgpu/parameters/ras_mask", O_RDONLY);
	CU_ASSERT_EQUAL(ret, 0);

	for (i = 0; i < devices_count; i++) {
		set_test_card(i);

		ret = amdgpu_query_info(device_handle, AMDGPU_INFO_RAS_ENABLED_FEATURES,
				sizeof(features), &features);
		CU_ASSERT_EQUAL(ret, 0);

		sprintf(path, "%s%s", get_ras_debugfs_root(), "ras_ctrl");
		ret = is_file_ok(path, O_WRONLY);
		CU_ASSERT_EQUAL(ret, 0);

		sprintf(path, "%s%s", get_ras_sysfs_root(), "features");
		ret = is_file_ok(path, O_RDONLY);
		CU_ASSERT_EQUAL(ret, 0);

		for (j = 0; j < AMDGPU_RAS_BLOCK__LAST; j++) {
			ret = amdgpu_ras_is_feature_supported(j);
			if (ret <= 0)
				continue;

			if (!((1 << j) & ras_block_mask_basic))
				continue;

			sprintf(path, "%s%s%s", get_ras_sysfs_root(), ras_block_str(j), "_err_count");
			ret = is_file_ok(path, O_RDONLY);
			CU_ASSERT_EQUAL(ret, 0);

			sprintf(path, "%s%s%s", get_ras_debugfs_root(), ras_block_str(j), "_err_inject");
			ret = is_file_ok(path, O_WRONLY);
			CU_ASSERT_EQUAL(ret, 0);
		}
	}
}
