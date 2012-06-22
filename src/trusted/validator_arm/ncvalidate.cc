/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/ncvalidate.h"


#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_arm/cpuid_arm.h"
#include "native_client/src/trusted/validator_arm/model.h"
#include "native_client/src/trusted/validator_arm/validator.h"
#include "native_client/src/trusted/validator/ncvalidate.h"

using nacl_arm_val::SfiValidator;
using nacl_arm_val::CodeSegment;
using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;
using nacl_arm_dec::kRegisterStack;
using std::vector;

class EarlyExitProblemSink : public nacl_arm_val::ProblemSink {
 private:
  bool problems_;

 public:
  EarlyExitProblemSink() : nacl_arm_val::ProblemSink(), problems_(false) {}
  virtual bool should_continue() {
    return !problems_;
  }

 protected:
  virtual void ReportProblemInternal(
      uint32_t vaddr,
      nacl_arm_val::ValidatorProblem problem,
      nacl_arm_val::ValidatorProblemMethod method,
      nacl_arm_val::ValidatorProblemUserData user_data) {
    UNREFERENCED_PARAMETER(vaddr);
    UNREFERENCED_PARAMETER(problem);
    UNREFERENCED_PARAMETER(method);
    UNREFERENCED_PARAMETER(user_data);

    problems_ = true;
  }
};

static NaClValidationStatus ValidatorCopyNotImplemented(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *cpu_features,
    NaClCopyInstructionFunc copy_func) {
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(data_old);
  UNREFERENCED_PARAMETER(data_new);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(cpu_features);
  UNREFERENCED_PARAMETER(copy_func);
  return NaClValidationFailedNotImplemented;
}

static NaClValidationStatus ValidatorCodeReplacementNotImplemented(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *cpu_features) {
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(data_old);
  UNREFERENCED_PARAMETER(data_new);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(cpu_features);
  return NaClValidationFailedNotImplemented;
}

EXTERN_C_BEGIN

int NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t size) {
  SfiValidator validator(
      16,  // bytes per bundle
      1U * 1024 * 1024 * 1024,  // bytes of code space
      1U * 1024 * 1024 * 1024,  // bytes of data space
      RegisterList(Register(9)),
      RegisterList(kRegisterStack));

  EarlyExitProblemSink sink;

  vector<CodeSegment> segments;
  segments.push_back(CodeSegment(mbase, vbase, size));

  bool success = validator.validate(segments, &sink);
  if (!success) return 2;  // for compatibility with old validator
  return 0;
}

static NaClValidationStatus ApplyValidatorArm(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int stubout_mode,
    int readonly_text,
    const NaClCPUFeatures *cpu_features,
    struct NaClValidationCache *cache) {
  UNREFERENCED_PARAMETER(cpu_features);
  /* The ARM validator is currently unsafe w.r.t. caching. */
  UNREFERENCED_PARAMETER(cache);

  if (stubout_mode)
    return NaClValidationFailedNotImplemented;
  if (readonly_text)
    return NaClValidationFailedNotImplemented;

  return ((0 == NCValidateSegment(data, guest_addr, size))
           ? NaClValidationSucceeded : NaClValidationFailed);
}

static struct NaClValidatorInterface validator = {
  ApplyValidatorArm,
  ValidatorCopyNotImplemented,
  ValidatorCodeReplacementNotImplemented,
};

const struct NaClValidatorInterface *NaClValidatorCreateArm() {
  return &validator;
}

EXTERN_C_END
