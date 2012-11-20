/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <climits>
#include <limits>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/validator_arm/cpuid_arm.h"
#include "native_client/src/trusted/validator_arm/model.h"
#include "native_client/src/trusted/validator_arm/validator.h"
#include "native_client/src/trusted/validator/ncvalidate.h"

using nacl_arm_val::SfiValidator;
using nacl_arm_val::CodeSegment;
using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;
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

static inline bool IsAligned(intptr_t value) {
  return (value & (NACL_BLOCK_SHIFT - 1)) == 0;
}

static inline bool IsAlignedPtr(void* ptr) {
  return IsAligned(reinterpret_cast<intptr_t>(ptr));
}

static const uintptr_t kBytesPerBundle = 1 << NACL_BLOCK_SHIFT;
static const uintptr_t kBytesOfCodeSpace = 1U * 1024 * 1024 * 1024;
static const uintptr_t kBytesOfDataSpace = 1U * 1024 * 1024 * 1024;

// The following checks should have been checked at a higher level, any error
// here is a break of the validator's preconditions. Don't try to recover.
static inline void CheckAddressOverflow(uint8_t* ptr, size_t size) {
  CHECK(ptr < std::numeric_limits<uint8_t *>::max() - size);
}

static inline void CheckAddressAlignAndOverflow(uint8_t* ptr, size_t size) {
  CHECK(IsAlignedPtr(ptr));
  CHECK(IsAligned(size));
  CheckAddressOverflow(ptr, size);
}

static NaClValidationStatus ValidatorCopyArm(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *cpu_features,
    NaClCopyInstructionFunc copy_func) {
  UNREFERENCED_PARAMETER(cpu_features);
  CheckAddressAlignAndOverflow((uint8_t *) guest_addr, size);
  CheckAddressOverflow(data_old, size);
  CheckAddressOverflow(data_new, size);

  CodeSegment dest_code(data_old, guest_addr, size);
  CodeSegment source_code(data_new, guest_addr, size);
  EarlyExitProblemSink sink;
  SfiValidator validator(
      kBytesPerBundle,
      kBytesOfCodeSpace,
      kBytesOfDataSpace,
      RegisterList(Register::Tp()),
      RegisterList(Register::Sp()));

  bool success = validator.CopyCode(source_code, dest_code, copy_func,
                                    &sink);
  return success ? NaClValidationSucceeded : NaClValidationFailed;
}

static NaClValidationStatus ValidatorCodeReplacementArm(
    uintptr_t guest_addr,
    uint8_t *data_old,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *cpu_features) {
  UNREFERENCED_PARAMETER(cpu_features);
  CheckAddressAlignAndOverflow((uint8_t *) guest_addr, size);
  CheckAddressOverflow(data_old, size);
  CheckAddressOverflow(data_new, size);

  CodeSegment new_code(data_new, guest_addr, size);
  CodeSegment old_code(data_old, guest_addr, size);
  EarlyExitProblemSink sink;
  SfiValidator validator(
      kBytesPerBundle,
      kBytesOfCodeSpace,
      kBytesOfDataSpace,
      RegisterList(Register::Tp()),
      RegisterList(Register::Sp()));

  bool success = validator.ValidateSegmentPair(old_code, new_code,
                                               &sink);
  return success ? NaClValidationSucceeded : NaClValidationFailed;
}

EXTERN_C_BEGIN

static int NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t size) {
  SfiValidator validator(
      kBytesPerBundle,
      kBytesOfCodeSpace,
      kBytesOfDataSpace,
      RegisterList(Register::Tp()),
      RegisterList(Register::Sp()));

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
  CheckAddressAlignAndOverflow((uint8_t *) guest_addr, size);
  CheckAddressOverflow(data, size);
  CheckAddressOverflow(data, size);

  if (stubout_mode)
    return NaClValidationFailedNotImplemented;
  if (readonly_text)
    return NaClValidationFailedNotImplemented;

  return ((0 == NCValidateSegment(data, guest_addr, size))
           ? NaClValidationSucceeded : NaClValidationFailed);
}

static struct NaClValidatorInterface validator = {
  ApplyValidatorArm,
  ValidatorCopyArm,
  ValidatorCodeReplacementArm,
};

const struct NaClValidatorInterface *NaClValidatorCreateArm() {
  return &validator;
}

/*
 * It should be moved to be part of sel_ldr, not the validator.
 */
int NaClCopyInstruction(uint8_t *dst, uint8_t *src, uint8_t sz) {
  CHECK(sz == nacl_arm_dec::kArm32InstSize / CHAR_BIT);
  *(volatile uint32_t*) dst = *(volatile uint32_t*) src;
  // Don't invalidate i-cache on every instruction update.
  // CPU executing partially updated code doesn't look like a problem,
  // as we know for sure that both old and new instructions are safe,
  // so is their mix (single instruction update is atomic).
  // We just have to make sure that unintended fallthrough doesn't
  // happen, and we don't change position of guard instructions.
  // Problem is that code is mapped for execution at different address
  // that one we use here, and ARM usually use virtually indexed caches,
  // so we couldn't invalidate correctly anyway.
  return 0;
}

EXTERN_C_END
