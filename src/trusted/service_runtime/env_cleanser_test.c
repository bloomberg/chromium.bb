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
#include <string.h>

#if defined(HAVE_SDL)
#  include <SDL.h>
#endif

#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/env_cleanser_test.h"

static char const *const kBogusEnvs[] = {
  "FOOBAR",
  "QUUX",
  "USER=bsy",
  "HOME=/home/bsy",
  "PATH=/home/bsy/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin",
  "LD_LIBRARY_PATH=.:/usr/bsy/lib",
  NULL,
};

static char const *const kValidEnvs[] = {
  "LANG=en_us.UTF-8",
  "LC_MEASUREMENT=en_US.UTF-8",
  "LC_PAPER=en_US.UTF-8@legal",
  "LC_TIME=%a, %B %d, %Y",
  NULL,
};

static char const *const kMurkyEnv[] = {
  "FOOBAR",
  "LC_TIME=%a, %B %d, %Y",
  "QUUX",
  "USER=bsy",
  "LC_PAPER=en_US.UTF-8@legal",
  "HOME=/home/bsy",
  "PATH=/home/bsy/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin",
  "LANG=en_us.UTF-8",
  "LC_MEASUREMENT=en_US.UTF-8",
  "LD_LIBRARY_PATH=.:/usr/bsy/lib",
  NULL,
};

static char const *const kFilteredEnv[] = {
  "LANG=en_us.UTF-8",
  "LC_MEASUREMENT=en_US.UTF-8",
  "LC_PAPER=en_US.UTF-8@legal",
  "LC_TIME=%a, %B %d, %Y",
  NULL,
};

int StrInStrTbl(char const *str, char const *const *tbl) {
  int i;

  for (i = 0; NULL != tbl[i]; ++i) {
    if (!strcmp(str, tbl[i])) {
      return 1;
    }
  }
  return 0;
}

int StrTblsHaveSameEntries(char const *const *tbl1, char const *const *tbl2) {
  int i;
  int num_left;
  int num_right;

  if (NULL == tbl1) {
    num_left = 0;
  } else {
    for (num_left = 0; NULL != tbl1[num_left]; ++num_left) {
    }
  }
  if (NULL == tbl2) {
    num_right = 0;
  } else {
    for (num_right = 0; NULL != tbl2[num_right]; ++num_right) {
    }
  }
  if (num_left != num_right) {
    return 0;
  }
  if (0 == num_left) {
    return 1;
  }
  for (i = 0; NULL != tbl1[i]; ++i) {
    if (!StrInStrTbl(tbl1[i], tbl2)) {
      return 0;
    }
  }
  for (i = 0; NULL != tbl2[i]; ++i) {
    if (!StrInStrTbl(tbl2[i], tbl1)) {
      return 0;
    }
  }
  return 1;
}

void PrintStrTbl(char const *name, char const *const *tbl) {
  printf("\n%s\n", name);
  printf("--------\n");
  for (; NULL != *tbl; ++tbl) {
    printf("%s\n", *tbl);
  }
  printf("--------\n");
}

int main(int ac, char **av) {
  int errors = 0;
  int i;
  struct NaClEnvCleanser nec;

  /* main's type signature is constrained by SDL */
  UNREFERENCED_PARAMETER(ac);
  UNREFERENCED_PARAMETER(av);

  printf("Environment Cleanser Test\n\n");
  printf("\nWhitelist self-check\n\n");
  for (i = 0; NULL != kNaClEnvWhitelist[i]; ++i) {
    printf("Checking %s\n", kNaClEnvWhitelist[i]);
    if (0 == NaClEnvInWhitelist(kNaClEnvWhitelist[i])) {
      ++errors;
      printf("ERROR\n");
    } else {
      printf("OK\n");
    }
  }

  printf("\nNon-whitelisted entries\n\n");

  for (i = 0; NULL != kBogusEnvs[i]; ++i) {
    printf("Checking %s\n", kBogusEnvs[i]);
    if (0 != NaClEnvInWhitelist(kBogusEnvs[i])) {
      ++errors;
      printf("ERROR\n");
    } else {
      printf("OK\n");
    }
  }

  printf("\nValid environment entries\n\n");

  for (i = 0; NULL != kValidEnvs[i]; ++i) {
    printf("Checking %s\n", kValidEnvs[i]);
    if (0 == NaClEnvInWhitelist(kValidEnvs[i])) {
      ++errors;
      printf("ERROR\n");
    } else {
      printf("OK\n");
    }
  }

  printf("\nEnvironment Filtering\n");
  NaClEnvCleanserCtor(&nec);
  if (!NaClEnvCleanserInit(&nec, kMurkyEnv)) {
    printf("FAILED: NaClEnvCleanser Init failed\n");
    ++errors;
  } else {
    if (!StrTblsHaveSameEntries(NaClEnvCleanserEnvironment(&nec),
                                kFilteredEnv)) {
      printf("ERROR: filtered env wrong\n");
      ++errors;

      PrintStrTbl("Original environment", kMurkyEnv);
      PrintStrTbl("Filtered environment", NaClEnvCleanserEnvironment(&nec));
      PrintStrTbl("Expected environment", kFilteredEnv);
    } else {
      printf("OK\n");
    }
  }
  NaClEnvCleanserDtor(&nec);

  printf("%s\n", (0 == errors) ? "PASSED" : "FAILED");
  return 0 != errors;
}
