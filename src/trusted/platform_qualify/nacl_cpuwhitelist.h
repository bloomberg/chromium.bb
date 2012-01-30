/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_CPUWHITELIST
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_CPUWHITELIST

/* NOTES:
 * The blacklist/whitelist go in an array which must be kept SORTED
 * as it is passed to bsearch.
 * An x86 CPU ID string must be 20 bytes plus a '\0'.
 */
#define NACL_BLACKLIST_TEST_ENTRY "NaClBlacklistTest123"

extern Bool NaCl_ThisCPUIsWhitelisted();
extern Bool NaCl_ThisCPUIsBlacklisted();

extern Bool NaCl_VerifyBlacklist();
extern Bool NaCl_VerifyWhitelist();

extern Bool NaCl_CPUIsWhitelisted(const char *myid);
extern Bool NaCl_CPUIsBlacklisted(const char *myid);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_CPUWHITELIST */
