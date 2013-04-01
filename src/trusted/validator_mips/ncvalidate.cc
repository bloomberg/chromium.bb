/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_mips/ncvalidate.h"

#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/arch/mips/sel_ldr_mips.h"
#include "native_client/src/trusted/cpu_features/arch/mips/cpu_mips.h"
#include "native_client/src/trusted/validator_mips/model.h"
#include "native_client/src/trusted/validator_mips/validator.h"

using nacl_mips_val::SfiValidator;
using nacl_mips_val::CodeSegment;
using nacl_mips_dec::RegisterList;
using std::vector;


class EarlyExitProblemSink : public nacl_mips_val::ProblemSink {
 private:
  bool problems_;

 public:
  EarlyExitProblemSink() : nacl_mips_val::ProblemSink(), problems_(false) {}

  virtual void ReportProblem(uint32_t vaddr,
                              nacl_mips_dec::SafetyLevel safety,
                              const nacl::string &problem_code,
                              uint32_t ref_vaddr) {
    UNREFERENCED_PARAMETER(vaddr);
    UNREFERENCED_PARAMETER(safety);
    UNREFERENCED_PARAMETER(problem_code);
    UNREFERENCED_PARAMETER(ref_vaddr);

    problems_ = true;
  }
  virtual bool ShouldContinue() {
    return !problems_;
  }
};


class StuboutProblemSink : public nacl_mips_val::ProblemSink {
 private:
  bool problems_;
  uint32_t const kNaClFullStop;

 public:
  StuboutProblemSink() : nacl_mips_val::ProblemSink(), problems_(false),
                                             kNaClFullStop(NACL_HALT_OPCODE) {}

  virtual void ReportProblem(uint32_t vaddr,
                              nacl_mips_dec::SafetyLevel safety,
                              const nacl::string &problem_code,
                              uint32_t ref_vaddr) {
    UNREFERENCED_PARAMETER(safety);
    UNREFERENCED_PARAMETER(problem_code);
    UNREFERENCED_PARAMETER(ref_vaddr);
    stub_out_instr(vaddr);

    problems_ = true;
  }
  virtual bool ShouldContinue() {
    return true;
  }

 private:
  void stub_out_instr(uint32_t vaddr) {
#ifdef __BIG_ENDIAN__
    CHECK(0);
#endif
    *reinterpret_cast<uint32_t *>(vaddr) = kNaClFullStop;
  }
};

EXTERN_C_BEGIN

int NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t size,
                      bool stubout_mode) {
  SfiValidator validator(
      16,                              // 64,  // bytes per bundle
      1U * NACL_DATA_SEGMENT_START,    // bytes of code space
      1U * (1 << NACL_MAX_ADDR_BITS),  // bytes of data space // keep in sync w/
                                       // SConstruct: irt_compatible_rodata_addr
      RegisterList::ReservedRegs(),    // read only register(s)
      RegisterList::DataAddrRegs());   // data addressing register(s)
  bool success = false;

  vector<CodeSegment> segments;
  segments.push_back(CodeSegment(mbase, vbase, size));

  if (stubout_mode) {
    StuboutProblemSink sink;
    success = validator.Validate(segments, &sink);
  } else {
    EarlyExitProblemSink sink;
    success = validator.Validate(segments, &sink);
  }
  if (!success) return 2;

  return 0;
}

static NaClValidationStatus ApplyValidatorMips(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int stubout_mode,
    int readonly_text,
    const NaClCPUFeatures *cpu_features,
    const struct NaClValidationMetadata *metadata,
    struct NaClValidationCache *cache) {
  NaClValidationStatus status = NaClValidationFailedNotImplemented;
  UNREFERENCED_PARAMETER(cpu_features);
  UNREFERENCED_PARAMETER(metadata);
  UNREFERENCED_PARAMETER(cache);
  if (stubout_mode) {
    NCValidateSegment(data, guest_addr, size, true);
    status = NaClValidationSucceeded;
  } else if (readonly_text) {
    status = NaClValidationFailedNotImplemented;
  } else {
    status = ((0 == NCValidateSegment(data, guest_addr, size, false))
                  ? NaClValidationSucceeded : NaClValidationFailed);
  }
  return status;
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

static struct NaClValidatorInterface validator = {
  ApplyValidatorMips,
  ValidatorCopyNotImplemented,
  ValidatorCodeReplacementNotImplemented,
  sizeof(NaClCPUFeaturesMips),
  NaClSetAllCPUFeaturesMips,
  NaClGetCurrentCPUFeaturesMips,
  NaClFixCPUFeaturesMips,
};

const struct NaClValidatorInterface *NaClValidatorCreateMips() {
  return &validator;
}

/*
 * When safe instruction copying gets implemented for MIPS, it should be
 * moved to be part of sel_ldr, not the validator.
 */
int NaClCopyInstruction(uint8_t *dst, uint8_t *src, uint8_t sz) {
  UNREFERENCED_PARAMETER(dst);
  UNREFERENCED_PARAMETER(src);
  UNREFERENCED_PARAMETER(sz);

  return 0;
}

EXTERN_C_END
