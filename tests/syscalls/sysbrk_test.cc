// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests excersize sysbrk() and sbrk().  The expected return values and
// the values for errno are as described here:
//   http://manpages.courier-mta.org/htmlman2/brk.2.html

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/nacl_syscalls.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "native_client/tests/syscalls/test.h"

/*
 * This is defined by the linker as the address of the end of our data segment.
 * That's where the break starts out by default.
 */
extern "C" {
  extern char end;
}

namespace {
// Note: these parameters to sysbrk are not supposed to be const.

// The defined error return address.
void* kSysbrkErrorAddress = reinterpret_cast<void*>(-1);

// This is an address outside of the 1Gb address range allowed for NaCl
// modules.
void* kIllegalBreakAddress = reinterpret_cast<void*>(~0U);

// The amount to increment/decrement sbrk
const intptr_t kAddrDelta = 65536;

// Make sure the current break address is non-0 when using sysbrk().
int TestCurrentBreakAddr() {
  START_TEST("TestCurrentBreakAddr");

  // Clear errno incase a previous function set it.
  errno = 0;

  void* break_addr = sysbrk(NULL);
  EXPECT(NULL != break_addr);
  EXPECT(kSysbrkErrorAddress != break_addr);
  EXPECT(0 == errno);
  END_TEST();
}


// Try to reset the program's break address to a legitimate value.
int TestSysbrk() {
  // Round up to the end of the page that's our last initial data page.
  // Then add 10MB for good measure to be out of the way of any allocations
  // that might have been done before we got here.
  void* const sysbrkBase = reinterpret_cast<void*>
    (((reinterpret_cast<uintptr_t>(&end) + 0xffff) & -0x10000) + (10 << 20));

  START_TEST("TestSysbrk");

  // Clear errno incase a previous function set it.
  errno = 0;

  void* break_addr = sysbrk(sysbrkBase);
  EXPECT(NULL != break_addr);
  EXPECT(kSysbrkErrorAddress != break_addr);
  EXPECT(sysbrkBase == break_addr);
  EXPECT(0 == errno);
  END_TEST();
}

// Make sure the current break address changes correctly
// on expansion and contraction.
int TestSbrk() {
  START_TEST("TestSbrkExpandContract");

  // Clear errno incase a previous function set it.
  errno = 0;

  // Get the start address.
  char *start_addr = reinterpret_cast<char *>(sbrk(0));
  EXPECT(NULL != start_addr);
  EXPECT(kSysbrkErrorAddress != start_addr)
  EXPECT(0 == errno);

  errno = 0;

  // Return the previous (start) address on increment.
  char *inc_addr = reinterpret_cast<char *>(sbrk(kAddrDelta));
  EXPECT(inc_addr == start_addr);
  EXPECT(0 == errno);
  errno = 0;

  // Verify that we actually did increment.
  char *cur_addr = reinterpret_cast<char *>(sbrk(0));
  EXPECT(cur_addr == (start_addr + kAddrDelta));
  EXPECT(0 == errno);
  errno = 0;

  // Return the previous (cur) address on decrement.
  char *dec_addr = reinterpret_cast<char *>(sbrk(-kAddrDelta));
  EXPECT(dec_addr == cur_addr);
  EXPECT(0 == errno);
  errno = 0;

  // Verify that we actually did decrement and are back where we started.
  cur_addr = reinterpret_cast<char *>(sbrk(0));
  EXPECT(cur_addr == start_addr);
  EXPECT(0 == errno);

  END_TEST();
}


// Try to reset the program's break address to something illegal using sysbrk().
// When sysbrk() fails, it is supposed to return the old break address and set
// |errno| "to an appropriate value" (in this case, EINVAL).
int TestIllegalSysbrk() {
  START_TEST("TestIllegalSysbrk");

  // Clear errno incase a previous function set it.
  errno = 0;

  void* current_break = sysbrk(NULL);
  void* break_addr = sysbrk(kIllegalBreakAddress);
  /* sysbrk does not touch errno, only the sbrk wrapper would */
  EXPECT(0 == errno);
  EXPECT(NULL != break_addr);
  EXPECT(current_break == break_addr);
  END_TEST();
}

// Try to reset the program's break address to something illegal using sbrk().
int TestIllegalSbrk() {
  START_TEST("TestIllegalSbrk");

  // Clear errno incase a previous function set it.
  errno = 0;

  void* current_break = sbrk(0);
  /*
   * sbrk(reinterpret_cast<ptrdiff_t>(kIllegalBreakAddress))
   * would just decrement by 1 the break!
   */
  void* break_addr = sbrk((char *) kIllegalBreakAddress
                          - (char *) current_break);
  EXPECT((void *) (-1) == break_addr);
  EXPECT(errno == ENOMEM);
  EXPECT(current_break != break_addr);
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
  fail_count += TestIllegalSysbrk();
  fail_count += TestIllegalSbrk();
  return fail_count;
}
