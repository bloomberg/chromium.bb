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
 * nacl_cpuwhitelist_test.c
 */
#include "native_client/src/include/portability.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"
#include "native_client/src/trusted/platform_qualify/nacl_cpuwhitelist.h"

/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern char *GetCPUIDString();


static void CPUIDWhitelistUnitTests() {
  if (!NaCl_VerifyWhitelist()) {
    fprintf(stderr, "ERROR: whitelist malformed\n");
    exit(-1);
  }

  if (!NaCl_VerifyBlacklist()) {
    fprintf(stderr, "ERROR: blacklist malformed\n");
    exit(-1);
  }

  if (!NaCl_CPUIsWhitelisted(" FakeEntry0000000000")) {
    fprintf(stderr, "ERROR: whitelist search 1 failed\n");
    exit(-1);
  }
  if (!NaCl_CPUIsWhitelisted("GenuineIntel00000f43")) {
    fprintf(stderr, "ERROR: whitelist search 2 failed\n");
    exit(-1);
  }
  if (!NaCl_CPUIsWhitelisted("zFakeEntry0000000000")) {
    fprintf(stderr, "ERROR: whitelist search 3 failed\n");
    exit(-1);
  }
  if (NaCl_CPUIsWhitelisted("a")) {
    fprintf(stderr, "ERROR: whitelist search 4 didn't fail\n");
    exit(-1);
  }
  if (NaCl_CPUIsWhitelisted("")) {
    fprintf(stderr, "ERROR: whitelist search 5 didn't fail\n");
    exit(-1);
  }
  if (NaCl_CPUIsWhitelisted("zFakeEntry0000000001")) {
    fprintf(stderr, "ERROR: whitelist search 6 didn't fail\n");
    exit(-1);
  }
  if (NaCl_CPUIsWhitelisted("zFakeEntry00000000000")) {
    fprintf(stderr, "ERROR: whitelist search 7 didn't fail\n");
    exit(-1);
  }
  printf("All whitelist unit tests passed\n");
}

int main() {
  printf("white list ID: %s\n", GetCPUIDString());
  if (NaCl_ThisCPUIsWhitelisted()) {
    printf("this CPU is on the whitelist\n");
  } else {
    printf("this CPU is NOT on the whitelist\n");
  }
  if (NaCl_ThisCPUIsBlacklisted()) {
    printf("this CPU is on the blacklist\n");
  } else {
    printf("this CPU is NOT on the blacklist\n");
  }

  CPUIDWhitelistUnitTests();
  return 0;
}
