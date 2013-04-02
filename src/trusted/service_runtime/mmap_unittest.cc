/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>

#if NACL_LINUX
# include <sys/mman.h>
#elif NACL_OSX
# include <mach/mach.h>
#endif

#include "gtest/gtest.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/mmap_test_check.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
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

void MapShmFd(struct NaClApp *nap, uintptr_t addr, size_t shm_size) {
  struct NaClDescImcShm *shm_desc =
      (struct NaClDescImcShm *) malloc(sizeof(*shm_desc));
  ASSERT_TRUE(shm_desc);
  ASSERT_EQ(NaClDescImcShmAllocCtor(shm_desc, shm_size,
                                    /* executable= */ 0), 1);
  struct NaClDesc *desc = &shm_desc->base;
  int fd = NaClSetAvail(nap, desc);

  uintptr_t mapping_addr = (uint32_t) NaClSysMmapIntern(
      nap, (void *) addr, shm_size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_SHARED, fd, 0);
  ASSERT_EQ(mapping_addr, addr);
}

void MapFileFd(struct NaClApp *nap, uintptr_t addr, size_t file_size) {
  int host_fd;
#if NACL_WINDOWS
  // Open temporary file that is deleted automatically.
  const char *temp_prefix = "nacl_mmap_test_temp_";
  char *temp_filename = _tempnam("C:\\Windows\\Temp", temp_prefix);
  ASSERT_EQ(_sopen_s(&host_fd, temp_filename,
                     _O_RDWR | _O_CREAT | _O_TEMPORARY,
                     _SH_DENYNO, _S_IREAD | _S_IWRITE), 0);
#else
  char temp_filename[] = "/tmp/nacl_mmap_test_temp_XXXXXX";
  host_fd = mkstemp(temp_filename);
  ASSERT_GE(host_fd, 0);
#endif
  ASSERT_EQ(remove(temp_filename), 0);

  struct NaClHostDesc *host_desc =
      (struct NaClHostDesc *) malloc(sizeof(*host_desc));
  ASSERT_TRUE(host_desc);
  ASSERT_EQ(NaClHostDescPosixTake(host_desc, host_fd, NACL_ABI_O_RDWR), 0);
  struct NaClDesc *desc = (struct NaClDesc *) NaClDescIoDescMake(host_desc);

  // Fill the file.  On Windows, this is necessary to ensure that NaCl
  // gives us mappings from the file instead of PROT_NONE-fill.
  char *buf = new char[file_size];
  memset(buf, 0, file_size);
  ssize_t written = NACL_VTBL(NaClDesc, desc)->Write(desc, buf, file_size);
  ASSERT_EQ(written, (ssize_t) file_size);
  delete[] buf;

  int fd = NaClSetAvail(nap, desc);

  uintptr_t mapping_addr = (uint32_t) NaClSysMmapIntern(
      nap, (void *) addr, file_size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_SHARED, fd, 0);
  ASSERT_EQ(mapping_addr, addr);
}

void UnmapMemory(struct NaClApp *nap, uintptr_t addr, size_t size) {
  // Create dummy NaClAppThread.
  // TODO(mseaborn): Clean up so that this is not necessary.
  struct NaClAppThread thread;
  memset(&thread, 0xff, sizeof(thread));
  thread.nap = nap;

  ASSERT_EQ(NaClSysMunmap(&thread, (void *) addr, size), 0);

  uintptr_t sysaddr = NaClUserToSys(nap, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_RESERVE, 0, MEM_PRIVATE);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_NONE, MAP_PRIVATE);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_NONE, SM_EMPTY);
#else
# error Unsupported platform
#endif
}

// Test that shared memory mappings can be unmapped by
// NaClAddrSpaceFree().  These are different from private memory
// mappings because, on Windows, they must be freed with
// UnmapViewOfFile() rather than VirtualFree().
TEST_F(MmapTest, TestFreeingShmMappings) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // Create a shared memory descriptor of arbitrary size and map it at
  // an arbitrary address.
  MapShmFd(&app, 0x200000, 0x100000);

  // Check that we can deallocate the address space.
  NaClAddrSpaceFree(&app);
}

// Test that unmapping a shared memory mapping does not leave behind
// an address space hole.
TEST_F(MmapTest, TestUnmapShmMapping) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // sel_mem.c does not like having an empty memory map, so create a
  // dummy entry that we do not later remove.
  // TODO(mseaborn): Clean up so that this is not necessary.
  MapShmFd(&app, 0x400000, 0x10000);

  uintptr_t addr = 0x200000;
  size_t size = 0x100000;
  MapShmFd(&app, addr, size);

  uintptr_t sysaddr = NaClUserToSys(&app, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_READWRITE, MEM_MAPPED);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_READ | PROT_WRITE, MAP_SHARED);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_READ | VM_PROT_WRITE, SM_SHARED);
#else
# error Unsupported platform
#endif

  UnmapMemory(&app, addr, size);

  NaClAddrSpaceFree(&app);
}

// Test that unmapping a file mapping does not leave behind an address
// space hole.
TEST_F(MmapTest, TestUnmapFileMapping) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // sel_mem.c does not like having an empty memory map, so create a
  // dummy entry that we do not later remove.
  // TODO(mseaborn): Clean up so that this is not necessary.
  MapShmFd(&app, 0x400000, 0x10000);

  uintptr_t addr = 0x200000;
  size_t size = 0x100000;
  MapFileFd(&app, addr, size);

  uintptr_t sysaddr = NaClUserToSys(&app, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_READWRITE, MEM_MAPPED);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_READ | PROT_WRITE, MAP_SHARED);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_READ | VM_PROT_WRITE, SM_PRIVATE);
#else
# error Unsupported platform
#endif

  UnmapMemory(&app, addr, size);

  NaClAddrSpaceFree(&app);
}

// Test we can map an anonymous memory mapping and that unmapping it
// does not leave behind an address space hole.
TEST_F(MmapTest, TestUnmapAnonymousMemoryMapping) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // sel_mem.c does not like having an empty memory map, so create a
  // dummy entry that we do not later remove.
  // TODO(mseaborn): Clean up so that this is not necessary.
  MapShmFd(&app, 0x400000, 0x10000);

  // Create an anonymous memory mapping.
  uintptr_t addr = 0x200000;
  size_t size = 0x100000;
  uintptr_t mapping_addr = (uint32_t) NaClSysMmapIntern(
      &app, (void *) addr, size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_ANONYMOUS,
      -1, 0);
  ASSERT_EQ(mapping_addr, addr);

  uintptr_t sysaddr = NaClUserToSys(&app, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_READWRITE, MEM_PRIVATE);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_READ | VM_PROT_WRITE, SM_EMPTY);
#else
# error Unsupported platform
#endif

  UnmapMemory(&app, addr, size);

  NaClAddrSpaceFree(&app);
}

// Test that changing a protection of a shared memory mapping is reflected
// by the underlying OS memory mapping.
TEST_F(MmapTest, TestProtectShmMapping) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // sel_mem.c does not like having an empty memory map, so create a
  // dummy entry that we do not later remove.
  // TODO(mseaborn): Clean up so that this is not necessary.
  MapShmFd(&app, 0x400000, 0x10000);

  uintptr_t addr = 0x200000;
  size_t size = 0x100000;
  MapShmFd(&app, addr, size);

  uintptr_t sysaddr = NaClUserToSys(&app, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_READWRITE, MEM_MAPPED);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_READ | PROT_WRITE, MAP_SHARED);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_READ | VM_PROT_WRITE, SM_SHARED);
#else
# error Unsupported platform
#endif

  ASSERT_EQ(0, NaClSysMprotectInternal(
                   &app, (uint32_t) addr, size, NACL_ABI_PROT_NONE));

#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_NOACCESS, MEM_MAPPED);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_NONE, MAP_SHARED);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_NONE, SM_SHARED);
#else
# error Unsupported platform
#endif

  NaClAddrSpaceFree(&app);
}

// Test that changing a protection of a file mapping is reflected
// by the underlying OS memory mapping.
TEST_F(MmapTest, TestProtectFileMapping) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // sel_mem.c does not like having an empty memory map, so create a
  // dummy entry that we do not later remove.
  // TODO(mseaborn): Clean up so that this is not necessary.
  MapShmFd(&app, 0x400000, 0x10000);

  uintptr_t addr = 0x200000;
  size_t size = 0x100000;
  MapFileFd(&app, addr, size);

  uintptr_t sysaddr = NaClUserToSys(&app, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_READWRITE, MEM_MAPPED);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_READ | PROT_WRITE, MAP_SHARED);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_READ | VM_PROT_WRITE, SM_PRIVATE);
#else
# error Unsupported platform
#endif

  ASSERT_EQ(0, NaClSysMprotectInternal(
                   &app, (uint32_t) addr, size, NACL_ABI_PROT_NONE));

#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_NOACCESS, MEM_MAPPED);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_NONE, MAP_SHARED);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_NONE, SM_PRIVATE);
#else
# error Unsupported platform
#endif

  NaClAddrSpaceFree(&app);
}

// Test that changing a protection of a anonymous memory mapping is
// reflected by the underlying OS memory mapping.
TEST_F(MmapTest, TestProtectAnonymousMemory) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  // sel_mem.c does not like having an empty memory map, so create a
  // dummy entry that we do not later remove.
  // TODO(mseaborn): Clean up so that this is not necessary.
  MapShmFd(&app, 0x400000, 0x10000);

  // Create an anonymous memory mapping.
  uintptr_t addr = 0x200000;
  size_t size = 0x100000;
  uintptr_t mapping_addr = (uint32_t) NaClSysMmapIntern(
      &app, (void *) addr, size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_ANONYMOUS,
      -1, 0);
  ASSERT_EQ(mapping_addr, addr);

  uintptr_t sysaddr = NaClUserToSys(&app, addr);
#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_READWRITE, MEM_PRIVATE);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_READ | VM_PROT_WRITE, SM_EMPTY);
#else
# error Unsupported platform
#endif

  ASSERT_EQ(0, NaClSysMprotectInternal(
                   &app, (uint32_t) addr, size, NACL_ABI_PROT_NONE));

#if NACL_WINDOWS
  CheckMapping(sysaddr, size, MEM_COMMIT, PAGE_NOACCESS, MEM_PRIVATE);
#elif NACL_LINUX
  CheckMapping(sysaddr, size, PROT_NONE, MAP_PRIVATE);
#elif NACL_OSX
  CheckMapping(sysaddr, size, VM_PROT_NONE, SM_EMPTY);
#else
# error Unsupported platform
#endif

  NaClAddrSpaceFree(&app);
}

// NaCl uses SysV shared memory only on Linux, because X Windows
// depends on it.
#if NACL_LINUX
TEST_F(MmapTest, TestSysvShmMapping) {
  struct NaClApp app;
  ASSERT_EQ(NaClAppCtor(&app), 1);
  ASSERT_EQ(NaClAllocAddrSpace(&app), LOAD_OK);

  size_t shm_size = 0x12000;
  size_t rounded_size = NaClRoundAllocPage(shm_size);
  ASSERT_EQ(rounded_size, (size_t) 0x20000);

  struct NaClDescSysvShm *shm_desc =
      (struct NaClDescSysvShm *) malloc(sizeof(*shm_desc));
  ASSERT_TRUE(NaClDescSysvShmCtor(shm_desc, shm_size));
  struct NaClDesc *desc = &shm_desc->base;
  int fd = NaClSetAvail(&app, desc);

  uintptr_t mapping_addr = 0x200000;

  // First, map something with PROT_READ, so that we can later check
  // that this is correctly overwritten by PROT_READ|PROT_WRITE and
  // PROT_NONE mappings.
  uintptr_t result_addr = (uint32_t) NaClSysMmapIntern(
      &app, (void *) mapping_addr, shm_size,
      NACL_ABI_PROT_READ,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_PRIVATE | NACL_ABI_MAP_ANONYMOUS,
      -1, 0);
  ASSERT_EQ(result_addr, mapping_addr);

  result_addr = (uint32_t) NaClSysMmapIntern(
      &app, (void *) mapping_addr, shm_size,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_FIXED | NACL_ABI_MAP_SHARED, fd, 0);
  ASSERT_EQ(result_addr, mapping_addr);
  uintptr_t sysaddr = NaClUserToSys(&app, mapping_addr);
  // We should see two mappings.  The first is the SysV shared memory
  // segment.  The second is padding upto the 64k page size.
  CheckMapping(sysaddr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED);
  CheckMapping(sysaddr + shm_size, rounded_size - shm_size,
               PROT_NONE, MAP_PRIVATE);

  NaClDescUnref(desc);
  NaClAddrSpaceFree(&app);
}
#endif

#endif /* NACL_ARCH(NACL_BUILD_ARCH) != NACL_arm */
