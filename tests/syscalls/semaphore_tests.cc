/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <semaphore.h>

#include <errno.h>
#include <unistd.h>

#include <cstdlib>
#include <limits>

#include "native_client/tests/syscalls/test.h"

// Test error conditions of sem_init and return the number of failed checks.
//
// According to the man page on Linux:
// ===================================
// RETURN VALUE
//        sem_init() returns 0 on success; on error, -1 is returned, and errno
//        is set to indicate the error.
//
// ERRORS
//        EINVAL value exceeds SEM_VALUE_MAX.
//
//        ENOSYS pshared is non-zero, but the system does  not  support
//               process-shared semaphores (see sem_overview(7)).
// ===================================
// However, pshared is not supported in NaCl currently, so a non-zero pshared
// value should yield an error (EINVAL).
int TestSemInitErrors() {
  START_TEST("sem_init error conditions");
  // First, make sure that it is possible to exceed SEM_VALUE_MAX
  // for this test, otherwise we can't cause this failure mode.
  EXPECT(SEM_VALUE_MAX < std::numeric_limits<unsigned int>::max());

  sem_t my_semaphore;

  // Create a value just beyond SEM_VALUE_MAX
  const unsigned int sem_max_plus_1 = SEM_VALUE_MAX + 1;

  // sem_init should return -1 and errno should equal EINVAL
  EXPECT(-1 == sem_init(&my_semaphore, 0, sem_max_plus_1));
  EXPECT(EINVAL == errno);

  // Try with the largest possible unsigned int.
  EXPECT(-1 == sem_init(&my_semaphore,
                        0,
                        std::numeric_limits<unsigned int>::max()));
  EXPECT(EINVAL == errno);

  // NaCl semaphores do not currently support the pshared option, so this should
  // fail.  If pshared gets added, we should begin testing for it.
  EXPECT(-1 == sem_init(&my_semaphore, 1, 0));
  EXPECT(EINVAL == errno);

  END_TEST();
}

int main() {
  int fail_count = 0;
  fail_count += TestSemInitErrors();

  std::exit(fail_count);
}
