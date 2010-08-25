// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// These tests excersize sysbrk() and sbrk().  The expected return values and
// the values for errno are as described here:
//   http://manpages.courier-mta.org/htmlman2/brk.2.html

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/nacl_syscalls.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "native_client/tests/syscalls/test.h"

namespace {
// Note: these parameters to sysbrk are not supposed to be const.

// This is an address inside the 1Gb allowable limit for NaCl modules.
// Really, we should get a legitimate address range from getrlimit() or
// pass a valid range in on the command line.
void* kSysbreakBase = reinterpret_cast<void*>(0x800000);

// The defined error return address.
void* kSysbrkErrorAddress = reinterpret_cast<void*>(-1);

// This is an address outside of the 1Gb address range allowed for NaCl
// modules.
void* kIllegalBreakAddress = reinterpret_cast<void*>(~0U);

// Make sure the current break address is non-0 when using sysbrk().
int TestCurrentBreakAddr() {
  START_TEST("TestCurrentBreakAddr");
  void* break_addr = sysbrk(NULL);
  EXPECT(NULL != break_addr);
  EXPECT(kSysbrkErrorAddress != break_addr);
  EXPECT(0 == errno);
  END_TEST();
}

// Make sure the current break address is non-0 when using sbrk().
int TestSbrk() {
  START_TEST("TestSbrk");
  void* break_addr = sbrk(NULL);
  EXPECT(NULL != break_addr);
  EXPECT(kSysbrkErrorAddress != break_addr)
  EXPECT(0 == errno);
  END_TEST();
}

// Try to reset the program's break address to a legitimate value.
int TestSysbrk() {
  START_TEST("TestSysbrk");
  void* break_addr = sysbrk(kSysbreakBase);
  EXPECT(NULL != break_addr);
  EXPECT(kSysbrkErrorAddress != break_addr);
  EXPECT(kSysbreakBase == break_addr);
  EXPECT(0 == errno);
  END_TEST();
}

// Try to reset the program's break address to something illegal using sysbrk().
// When sysbrk() fails, it is supposed to return the old break address and set
// |errno| "to an appropriate value" (in this case, EINVAL).
int TestIllegalSysbrk() {
  START_TEST("TestIllegalSysbrk");
  void* current_break = sysbrk(NULL);
  void* break_addr = sysbrk(kIllegalBreakAddress);
  EXPECT(NULL != break_addr);
  EXPECT(errno == EINVAL);
  EXPECT(current_break == break_addr);
  END_TEST();
}

// Try to reset the program's break address to something illegal using sbrk().
int TestIllegalSbrk() {
  START_TEST("TestIllegalSbrk");
  void* current_break = sbrk(NULL);
  void* break_addr = sbrk(reinterpret_cast<ptrdiff_t>(kIllegalBreakAddress));
  EXPECT(NULL != break_addr);
  EXPECT(errno == ENOMEM);
  EXPECT(current_break == break_addr);
  END_TEST();
}
}  // namespace

// Run through the complete sequence of sysbrk tests.  Sets the exit code to
// the number of failed tests.  Exit code 0 means all passed.
int main() {
  int fail_count = 0;
  fail_count += TestCurrentBreakAddr();
  fail_count += TestSysbrk();
  fail_count += TestSbrk();
  // TODO(bsy): uncomment the following two lines when bug 839 is fixed.  See
  // http://code.google.com/p/nativeclient/issues/detail?id=839
  // fail_count += TestIllegalSysbrk();
  // fail_count += TestIllegalSbrk();
  return fail_count;
}
