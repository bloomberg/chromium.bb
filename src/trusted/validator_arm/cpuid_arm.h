/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_CPUID_ARM_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_CPUID_ARM_H

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator/ncvalidate.h"


EXTERN_C_BEGIN

/* List of features supported by ARM CPUs. */
typedef enum {
#define NACL_ARM_CPU_FEATURE(name) NACL_CONCAT(NaClCPUFeatureArm_, name),
#include "native_client/src/trusted/validator_arm/cpuid_arm_features.h"
#undef NACL_ARM_CPU_FEATURE
  /* Leave the following as the last entry. */
  NaClCPUFeatureArm_Max
} NaClCPUFeatureArmID;

typedef struct cpu_feature_struct_arm {
  char data[NaClCPUFeatureArm_Max];
} NaClCPUFeaturesArm;

/*
 * Platform-independent NaClValidatorInterface functions.
 */
void NaClSetAllCPUFeaturesArm(NaClCPUFeatures *features);
void NaClGetCurrentCPUFeaturesArm(NaClCPUFeatures *features);
int NaClFixCPUFeaturesArm(NaClCPUFeatures *features);

/*
 * Platform-dependent getter/setter.
 */
static INLINE int NaClGetCPUFeatureArm(const NaClCPUFeaturesArm *features,
                                       NaClCPUFeatureArmID id) {
  return features->data[id];
}

void NaClSetCPUFeatureArm(NaClCPUFeaturesArm *features, NaClCPUFeatureArmID id,
                          int state);
const char *NaClGetCPUFeatureArmName(NaClCPUFeatureArmID id);

/*
 * Platform-independent functions which are only used in platform-dependent
 * code.
 */
void NaClClearCPUFeaturesArm(NaClCPUFeaturesArm *features);
/*
 * TODO(jfb) The x86 CPU features also offers these functions, which are
 * currently not used on ARM: NaClCopyCPUFeaturesArm, NaClArchSupportedArm.
 */

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_CPUID_ARM_H */
