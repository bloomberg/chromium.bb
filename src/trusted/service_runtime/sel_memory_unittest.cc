/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Testing NativeClient cross-platfom memory management functions

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_log.h"

// TODO(robertm): eliminate need for this
#if NACL_WINDOWS
#include "native_client/src/trusted/service_runtime/win/mman.h"
#endif
#include "native_client/src/trusted/service_runtime/sel_memory.h"

#include "gtest/gtest.h"

class SelMemoryBasic : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();
  // TODO(gregoryd) - do we need a destructor here?
};

void SelMemoryBasic::SetUp() {
  NaClLogModuleInit();
}

void SelMemoryBasic::TearDown() {
  NaClLogModuleFini();
}


TEST_F(SelMemoryBasic, AllocationTests) {
  int res = 0;
  void *p = NULL;
  int size;

  size = 0x2001;  // not power of two - should be supported

  res = NaCl_page_alloc(&p, size);
  EXPECT_EQ(0, res);
  EXPECT_NE(static_cast<void *>(NULL), p);

  NaCl_page_free(p, size);
  p = NULL;

  // Try to allocate large memory block
  size = 256 * 1024 * 1024;  // 256M

  res = NaCl_page_alloc(&p, size);
  EXPECT_EQ(0, res);
  EXPECT_NE(static_cast<void *>(NULL), p);

  NaCl_page_free(p, size);
}

TEST_F(SelMemoryBasic, mprotect) {
  int res = 0;
  void *p = NULL;
  int size;
  char *addr;

  size = 0x100000;

  res = NaCl_page_alloc(&p, size);
  EXPECT_EQ(0, res);
  EXPECT_NE(static_cast<void *>(NULL), p);

  // TODO(gregoryd) - since ASSERT_DEATH is not supported for client projects,
  // we cannot use gUnit to test the protection. We might want to add some
  // internal processing (based on SEH/signals) at some stage

  res = NaCl_mprotect(p, size, PROT_READ |PROT_WRITE);
  EXPECT_EQ(0, res);
  addr = reinterpret_cast<char*>(p);
  addr[0] = '5';

  res = NaCl_mprotect(p, size, PROT_READ);
  EXPECT_EQ(0, res);
  EXPECT_EQ('5', addr[0]);

  res = NaCl_mprotect(p, size, PROT_READ|PROT_WRITE|PROT_EXEC);

  NaCl_page_free(p, size);
}
