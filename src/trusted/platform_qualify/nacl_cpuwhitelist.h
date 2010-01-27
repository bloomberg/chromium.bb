/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_CPUWHITELIST
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_CPUWHITELIST

extern bool NaCl_ThisCPUIsWhitelisted();
extern bool NaCl_ThisCPUIsBlacklisted();

extern bool NaCl_VerifyBlacklist();
extern bool NaCl_VerifyWhitelist();

extern bool NaCl_CPUIsWhitelisted(const char *myid);
extern bool NaCl_CPUIsBlacklisted(const char *myid);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_CPUWHITELIST */
