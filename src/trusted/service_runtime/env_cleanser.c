/*
 * Copyright 2009, Google Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/env_cleanser_test.h"

void NaClEnvCleanserCtor(struct NaClEnvCleanser *self) {
  self->cleansed_environ = (char const **) NULL;
}

/*
 * Environment variables names are from IEEE Std 1003.1-2001, with
 * additional ones from locale(7) from glibc / linux.  The entries
 * must be sorted, in ASCII order, for the bsearch to run correctly.
 */
/* static -- not static for testing */
char const *const kNaClEnvWhitelist[] = {
  "LANG",
  "LC_ADDRESS",
  "LC_ALL",
  "LC_COLLATE",
  "LC_CTYPE",
  "LC_IDENTIFICATION",
  "LC_MEASUREMENT",
  "LC_MESSAGES",
  "LC_MONETARY",
  "LC_NAME",
  "LC_NUMERIC",
  "LC_PAPER",
  "LC_TELEPHONE",
  "LC_TIME",
  "NACL_PLUGIN_DEBUG",  /* src/trusted/plugin/srpc/utility.cc */
  "NACL_SRPC_DEBUG",    /* src/shared/srpc/utility.c */
  NULL,
};

/* left arg is key, right arg is table entry */
static int EnvCmp(void const *vleft, void const *vright) {
  char const *left = *(char const *const *) vleft;
  char const *right = *(char const *const *) vright;
  char cleft, cright;

  while ((cleft = *left) == (cright = *right)
         && '\0' != cleft
         && '\0' != cright) {
    ++left;
    ++right;
  }
  if ('=' == cleft && '\0' == cright) {
    return 0;
  }
  return (0xff & cleft) - (0xff & cright);
}

int NaClEnvInWhitelist(char const *env_entry) {
  return NULL != bsearch((void const *) &env_entry,
                         (void const *) kNaClEnvWhitelist,
                         (sizeof kNaClEnvWhitelist
                          / sizeof kNaClEnvWhitelist[0]) - 1,  /* NULL */
                         sizeof kNaClEnvWhitelist[0],
                         EnvCmp);
}

/* PRE: sizeof(char *) is a power of 2 */

/*
 * Initializes the object with a filtered environment.
 *
 * May return false on errors, e.g., out-of-memory.
 */
int NaClEnvCleanserInit(struct NaClEnvCleanser *self, char const *const *envp) {
  char const *const *p;
  size_t num_env = 0;
  size_t ptr_bytes = 0;
  const size_t kMaxSize = ~(size_t) 0;
  const size_t ptr_size_mult_overflow_mask = ~(kMaxSize / sizeof *envp);
  char const **ptr_tbl;
  int env;

  /*
   * let n be a size_t.  if n & ptr_size_mult_overflow_mask is set,
   * then n*sizeof(void *) will have an arithmetic overflow.
   */

  if (NULL == envp || NULL == *envp) {
    self->cleansed_environ = NULL;
    return 1;
  }
  for (p = envp; NULL != *p; ++p) {
    if (!NaClEnvInWhitelist(*p)) {
      continue;
    }
    if (num_env == kMaxSize) {
      /* would overflow */
      return 0;
    }
    ++num_env;
  }

  /* pointer table -- NULL pointer terminated */
  if (0 != ((1 + num_env) & ptr_size_mult_overflow_mask)) {
    return 0;
  }
  ptr_bytes = (1 + num_env) * sizeof(*envp);

  ptr_tbl = (char const **) malloc(ptr_bytes);
  if (NULL == ptr_tbl) {
    return 0;
  }

  /* this assumes no other thread is tweaking envp */
  for (env = 0, p = envp; NULL != *p; ++p) {
    if (!NaClEnvInWhitelist(*p)) {
      continue;
    }
    ptr_tbl[env] = *p;
    ++env;
  }
  if (num_env != env) {
    free((void *) ptr_tbl);
    return 0;
  }
  ptr_tbl[env] = NULL;
  self->cleansed_environ = ptr_tbl;

  return 1;
}

char const *const *NaClEnvCleanserEnvironment(struct NaClEnvCleanser *self) {
  return (char const *const *) self->cleansed_environ;
}

void NaClEnvCleanserDtor(struct NaClEnvCleanser *self) {
  free((void *) self->cleansed_environ);
  self->cleansed_environ = NULL;
}
