/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl semaphore type (OSX)
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_OSX_NACL_SEMAPHORE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_OSX_NACL_SEMAPHORE_H_

#include <semaphore.h>

/*
* NOTE(gregoryd): following Gears in defining SEM_NAME_LEN:
* Gears: The docs claim that SEM_NAME_LEN should be defined.  It is not.
* However, by looking at the xnu source (bsd/kern/posix_sem.c),
* it defines PSEMNAMLEN to be 31 characters.  We'll use that value.
*/
#define SEM_NAME_LEN 31

struct NaClSemaphore {
  sem_t *sem_descriptor;
  /*
  * There are 62^30 possible names - should be enough.
  * 62 = 26*2 + 10 - the number of alphanumeric characters
  * 30 = SEM_NAME_LEN - 1
  */
  char sem_name[SEM_NAME_LEN];
};

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_OSX_NACL_SEMAPHORE_H_ */
