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

enum amdgpu_ras_error_type {
	AMDGPU_RAS_ERROR__NONE				= 0,
	AMDGPU_RAS_ERROR__SINGLE_CORRECTABLE		= 2,
	AMDGPU_RAS_ERROR__MULTI_UNCORRECTABLE		= 4,
	AMDGPU_RAS_ERROR__POISON			= 8,
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
#define DEFAULT_RAS_BLOCK_MASK_INJECT (1 << AMDGPU_RAS_BLOCK__UMC)
#define DEFAULT_RAS_BLOCK_MASK_QUERY (1 << AMDGPU_RAS_BLOCK__UMC)
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

static void __amdgpu_ras_inject_test(void)
{
	struct ras_debug_if data;
	int ret;
	int i;
	unsigned long ue, ce, ue_old, ce_old;

	data.op = 2;
	for (i = 0; i < AMDGPU_RAS_BLOCK__LAST; i++) {
		int timeout = 3;
		struct ras_inject_if inject = {
			.head = {
				.block = i,
				.type = AMDGPU_RAS_ERROR__MULTI_UNCORRECTABLE,
				.sub_block_index = 0,
				.name = "",
			},
			.address = 0,
			.value = 0,
		};

		if (amdgpu_ras_is_feature_enabled(i) <= 0)
			continue;

		if (!((1 << i) & ras_block_mask_inject))
			continue;

		data.inject = inject;

		ret = amdgpu_ras_query_err_count(i, &ue_old, &ce_old);
		CU_ASSERT_EQUAL(ret, 0);

		if (ret)
			continue;

		ret = amdgpu_ras_invoke(&data);
		CU_ASSERT_EQUAL(ret, 0);

		if (ret)
			continue;

loop:
		while (timeout > 0) {
			ret = amdgpu_ras_query_err_count(i, &ue, &ce);
			CU_ASSERT_EQUAL(ret, 0);

			if (ret)
				continue;
			if (ue_old != ue) {
				/*recovery takes ~10s*/
				sleep(10);
				break;
			}

			sleep(1);
			timeout -= 1;
		}

		CU_ASSERT_EQUAL(ue_old + 1, ue);
		CU_ASSERT_EQUAL(ce_old, ce);
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
