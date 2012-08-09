/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/mmap_test_check.h"

#include <mach/mach.h>
#include <stdio.h>

#include "native_client/src/include/nacl_assert.h"


void CheckMapping(uintptr_t addr, size_t size, int protect, int map_type) {
  vm_address_t r_start;
  vm_size_t r_size;
  vm_region_extended_info_data_t info;
  mach_msg_type_number_t info_count;
  memory_object_name_t object;
  kern_return_t result;

  r_start = (vm_address_t) addr;
  info_count = VM_REGION_EXTENDED_INFO_COUNT;

  result = vm_region_64(mach_task_self(), &r_start, &r_size,
                        VM_REGION_EXTENDED_INFO, (vm_region_info_t) &info,
                        &info_count, &object);
  ASSERT_EQ(result, KERN_SUCCESS);
  ASSERT_EQ(r_start, addr);
  ASSERT_EQ(r_size, size);
  ASSERT_EQ(info.protection, protect);
  // TODO(phosek): not sure whether this check is correct
  ASSERT_EQ(info.share_mode, map_type);
}
