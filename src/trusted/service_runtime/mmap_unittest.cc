/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gtest/gtest.h"

#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


class MmapTest : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();
};

void MmapTest::SetUp() {
  NaClNrdAllModulesInit();
}

void MmapTest::TearDown() {
  NaClNrdAllModulesFini();
}

// These tests are disabled for ARM because the ARM sandbox is
// zero-based, and sel_addrspace_arm.c does not work when allocating a
// non-zero-based region.
// TODO(mseaborn): Change sel_addrspace_arm.c to work with this test.
// However, for now, testing the Linux memory mapping code under
// x86-32 and x86-64 gives us good coverage of the Linux code in
// general.
#if NACL_ARCH(NACL_BUILD_ARCH) != NACL_arm

// Check that the untrusted address space really gets freed by trying
// to allocate and free a sandbox multiple times.  On a 32-bit system,
// this would fail if NaClAddrSpaceFree() were a no-op because it
// would run out of host address space.
TEST_F(MmapTest, TestFreeingAddressSpace) {
  // We use a small number of iterations because address space
  // allocation can be very slow on Windows (on the order of 1 second
  // on the 32-bit Windows bots).
  for (int count = 0; count < 4; count++) {
    struct NaClApp app;
    ASSERT_EQ(NaClAppCtor(&app), 1);
    ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);
    NaClAddrSpaceFree(&app);
  }
}

// Test that shared memory mappings can be unmapped by
// NaClAddrSpaceFree().  These are different from private memory
// mappings because, on Windows, they must be freed with
// UnmapViewOfFile() rather than VirtualFree().
TEST_F(MmapTest, TestFreeingFileMappings) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // Create a shared memory descriptor of arbitrary size.
  size_t shm_size = 0x100000;
  struct NaClDescImcShm *shm_desc =
      (struct NaClDescImcShm *) malloc(sizeof(*shm_desc));
  ASSERT_TRUE(shm_desc);
  ASSERT_EQ(NaClDescImcShmAllocCtor(shm_desc, shm_size,
                                    /* executable= */ 0), 1);
  struct NaClDesc *desc = &shm_desc->base;
  int fd = NaClSetAvail(&app, desc);

  // Map the shared memory descriptor at an arbitrary address.
  int32_t request_addr = 0x200000;
  int32_t mapping_addr = NaClCommonSysMmapIntern(
      &app, (void *) request_addr, shm_size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_SHARED, fd, 0);
  ASSERT_EQ(mapping_addr, request_addr);

  // Check that we can deallocate the address space.
  NaClAddrSpaceFree(&app);
}

#endif
