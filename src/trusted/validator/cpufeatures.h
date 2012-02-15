/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_CPUID_ARCH_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_CPUID_ARCH_H

#include "native_client/src/include/nacl_base.h"

#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_x86

#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"
typedef NaClCPUFeaturesX86 NaClCPUFeatures;

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

#include "native_client/src/trusted/validator_arm/cpuid_arm.h"
typedef NaClCPUFeaturesArm NaClCPUFeatures;

#else

#error Unknown platform!

#endif


#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_CPUID_ARCH_H */
