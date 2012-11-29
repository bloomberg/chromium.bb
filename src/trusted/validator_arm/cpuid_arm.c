/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_arm/cpuid_arm.h"


void NaClSetAllCPUFeaturesArm(NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast in this interface. */
  NaClCPUFeaturesArm *features = (NaClCPUFeaturesArm *) f;
  /* Pedantic: avoid using memset, as in x86's nacl_cpuid.c. */
  int id;
  /* Ensure any padding is zeroed. */
  NaClClearCPUFeaturesArm(features);
  for (id = 0; id < NaClCPUFeatureArm_Max; ++id) {
    NaClSetCPUFeatureArm(features, id, 1);
  }
}

void NaClGetCurrentCPUFeaturesArm(NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast in this interface. */
  NaClCPUFeaturesArm *features = (NaClCPUFeaturesArm *) f;
  features->data[NaClCPUFeatureArm_BOGUS] = 0;
}

/* This array defines the CPU feature model for fixed-feature CPU mode. */
static const int kFixedFeatureArmCPUModel[NaClCPUFeatureArm_Max] = {
  0 /* NaClCPUFeatureArm_BOGUS */
};

int NaClFixCPUFeaturesArm(NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast in this interface. */
  NaClCPUFeaturesArm *features = (NaClCPUFeaturesArm *) f;
  NaClCPUFeatureArmID fid;
  int rvalue = 1;

  for (fid = 0; fid < NaClCPUFeatureArm_Max; fid++) {
    if (kFixedFeatureArmCPUModel[fid]) {
      if (!NaClGetCPUFeatureArm(features, fid)) {
        /* This CPU is missing a required feature. */
        NaClLog(LOG_ERROR,
                "This CPU is missing a feature required by fixed-mode: %s\n",
                NaClGetCPUFeatureArmName(fid));
        rvalue = 0;  /* set return value to indicate failure */
      }
    } else {
      /* Feature is not in the fixed model.
       * Ensure cpu_features does not have it either.
       */
      NaClSetCPUFeatureArm(features, fid, 0);
    }
  }
  return rvalue;
}

void NaClSetCPUFeatureArm(NaClCPUFeaturesArm *f, NaClCPUFeatureArmID id,
                          int state) {
  f->data[id] = (char) state;
}

const char *NaClGetCPUFeatureArmName(NaClCPUFeatureArmID id) {
  static const char *kFeatureArmNames[NaClCPUFeatureArm_Max] = {
# define NACL_ARM_CPU_FEATURE(name) NACL_TO_STRING(name),
# include "native_client/src/trusted/validator_arm/cpuid_arm_features.h"
# undef NACL_ARM_CPU_FEATURE
  };
  return ((unsigned)id < NaClCPUFeatureArm_Max) ?
      kFeatureArmNames[id] : "INVALID";
}

void NaClClearCPUFeaturesArm(NaClCPUFeaturesArm *features) {
  memset(features, 0, sizeof(*features));
}
