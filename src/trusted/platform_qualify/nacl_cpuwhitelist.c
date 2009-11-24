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

/*
 * nacl_cpuwhitelist.c
 */
#include "native_client/src/include/portability.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"
#include "native_client/src/trusted/platform_qualify/nacl_cpuwhitelist.h"


static int idcmp(const void *s1, const void *s2) {
  return strncmp((char *)s1, *(char **)s2, kCPUIDStringLength);
}



static const char* const kNaclCpuBlacklist[] = {
/*          1         2        */
/* 12345678901234567890 + '\0' */
  " FakeEntry0000000000",
  " FakeEntry0000000001",
  "zFakeEntry0000000000",
};

static const char* const kNaclCpuWhitelist[] = {
/*          1         2        */
/* 12345678901234567890 + '\0' */
  " FakeEntry0000000000",
  " FakeEntry0000000001",
  " FakeEntry0000000002",
  " FakeEntry0000000003",
  " FakeEntry0000000004",
  " FakeEntry0000000005",
  "GenuineIntel00000f43",
  "zFakeEntry0000000000",
};



static bool VerifyCpuList(const char* const cpulist[], int n) {
  int i;
  /* check lengths */
  for (i = 0; i < n; i++) {
    if (strlen(cpulist[i]) != kCPUIDStringLength - 1) {
      return 0;
    }
  }

  /* check order */
  for (i = 1; i < n; i++) {
    if (strncmp(cpulist[i-1], cpulist[i], kCPUIDStringLength) >= 0) {
      return 0;
    }
  }

  return 1;
}


bool NaCl_VerifyBlacklist() {
  return VerifyCpuList(kNaclCpuBlacklist, NACL_ARRAY_SIZE(kNaclCpuBlacklist));
}


bool NaCl_VerifyWhitelist() {
  return VerifyCpuList(kNaclCpuWhitelist, NACL_ARRAY_SIZE(kNaclCpuWhitelist));
}


static bool IsCpuInList(const char *myid, const char* const cpulist[], int n) {
  return (NULL != bsearch(myid, cpulist, n, sizeof(char*), idcmp));
}


/* for testing */
bool NaCl_CPUIsWhitelisted(const char *myid) {
  return IsCpuInList(myid,
                     kNaclCpuWhitelist,
                     NACL_ARRAY_SIZE(kNaclCpuWhitelist));
}

/* for testing */
bool NaCl_CPUIsBlacklisted(const char *myid) {
  return IsCpuInList(myid,
                     kNaclCpuBlacklist,
                     NACL_ARRAY_SIZE(kNaclCpuBlacklist));
}


bool NaCl_ThisCPUIsWhitelisted() {
  const char* myid = GetCPUIDString();
  return IsCpuInList(myid,
                     kNaclCpuWhitelist,
                     NACL_ARRAY_SIZE(kNaclCpuWhitelist));
}

bool NaCl_ThisCPUIsBlacklisted() {
  const char* myid = GetCPUIDString();
  return IsCpuInList(myid,
                     kNaclCpuBlacklist,
                     NACL_ARRAY_SIZE(kNaclCpuBlacklist));
}
