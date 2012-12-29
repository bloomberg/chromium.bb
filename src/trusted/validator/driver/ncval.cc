/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "native_client/src/include/elf.h"
#include "native_client/src/include/elf_constants.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/driver/elf_load.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

using std::set;
using std::string;
using std::vector;


struct Jump {
  uint32_t from;
  uint32_t to;
  Jump(uint32_t from, uint32_t to) : from(from), to(to) {}
};


struct Error {
  uint32_t offset;
  string message;
  Error(uint32_t offset, string message) : offset(offset), message(message) {}
};


struct UserData {
  Segment segment;
  vector<Error> *errors;
  vector<Jump> *jumps;
  set<uint32_t> *bad_jump_targets;
  UserData(Segment segment,
           vector<Error> *errors,
           vector<Jump> *jumps,
           set<uint32_t> *bad_jump_targets)
      : segment(segment),
        errors(errors),
        jumps(jumps),
        bad_jump_targets(bad_jump_targets) {
  }
};


Bool ProcessInstruction(
    const uint8_t *begin, const uint8_t *end,
    uint32_t validation_info, void *user_data_ptr) {

  UserData &user_data = *reinterpret_cast<UserData *>(user_data_ptr);
  vector<Error> &errors = *user_data.errors;

  uint32_t offset = user_data.segment.vaddr +
                    static_cast<uint32_t>(begin - user_data.segment.data);

  // Presence of RELATIVE_8BIT or RELATIVE_32BIT indicates that instruction is
  // immediate jump or call.
  // We only record jumps that are in range, so we don't have to worry
  // about overflows.
  if ((validation_info & DIRECT_JUMP_OUT_OF_RANGE) == 0) {
    uint32_t program_counter = user_data.segment.vaddr +
                               static_cast<uint32_t>(end + 1 -
                                                     user_data.segment.data);
    if (validation_info & RELATIVE_8BIT) {
      program_counter += *reinterpret_cast<const int8_t *>(end);
      user_data.jumps->push_back(Jump(offset, program_counter));
    } else if (validation_info & RELATIVE_32BIT) {
      program_counter += *reinterpret_cast<const int32_t *>(end - 3);
      user_data.jumps->push_back(Jump(offset, program_counter));
    }
  }

  Bool result = (validation_info & (VALIDATION_ERRORS_MASK | BAD_JUMP_TARGET))
                ? FALSE
                : TRUE;

  // We clear bit for each processed error, and in the end if there are still
  // errors we did not report, we produce generic error message.
  // This way, if validator interface was changed (new error bits were
  // introduced), we at least wouldn't ignore them completely.

  // TODO(shcherbina): deal with code duplication somehow.

  if (validation_info & UNRECOGNIZED_INSTRUCTION) {
    validation_info &= ~UNRECOGNIZED_INSTRUCTION;
    errors.push_back(Error(offset, "unrecognized instruction"));
  }

  if (validation_info & DIRECT_JUMP_OUT_OF_RANGE) {
    validation_info &= ~DIRECT_JUMP_OUT_OF_RANGE;
    errors.push_back(Error(offset, "direct jump out of range"));
  }

  if (validation_info & CPUID_UNSUPPORTED_INSTRUCTION) {
    validation_info &= ~CPUID_UNSUPPORTED_INSTRUCTION;
    errors.push_back(Error(offset, "required CPU feature not found"));
  }

  if (validation_info & FORBIDDEN_BASE_REGISTER) {
    validation_info &= ~FORBIDDEN_BASE_REGISTER;
    errors.push_back(Error(offset, "improper memory address - bad base"));
  }

  if (validation_info & UNRESTRICTED_INDEX_REGISTER) {
    validation_info &= ~UNRESTRICTED_INDEX_REGISTER;
    errors.push_back(Error(offset, "improper memory address - bad index"));
  }

  if ((validation_info & BAD_RSP_RBP_PROCESSING_MASK) ==
      RESTRICTED_RBP_UNPROCESSED) {
    validation_info &= ~BAD_RSP_RBP_PROCESSING_MASK;
    errors.push_back(Error(offset, "improper %rbp sandboxing"));
  }

  if ((validation_info & BAD_RSP_RBP_PROCESSING_MASK) ==
      UNRESTRICTED_RBP_PROCESSED) {
    validation_info &= ~BAD_RSP_RBP_PROCESSING_MASK;
    errors.push_back(Error(offset, "improper %rbp sandboxing"));
  }

  if ((validation_info & BAD_RSP_RBP_PROCESSING_MASK) ==
      RESTRICTED_RSP_UNPROCESSED) {
    validation_info &= ~BAD_RSP_RBP_PROCESSING_MASK;
    errors.push_back(Error(offset, "improper %rsp sandboxing"));
  }

  if ((validation_info & BAD_RSP_RBP_PROCESSING_MASK) ==
      UNRESTRICTED_RSP_PROCESSED) {
    validation_info &= ~BAD_RSP_RBP_PROCESSING_MASK;
    errors.push_back(Error(offset, "improper %rsp sandboxing"));
  }

  if (validation_info & R15_MODIFIED) {
    validation_info &= ~R15_MODIFIED;
    errors.push_back(Error(offset, "error - %r15 is changed"));
  }

  if (validation_info & BPL_MODIFIED) {
    validation_info &= ~BPL_MODIFIED;
    errors.push_back(Error(offset, "error - %bpl or %bp is changed"));
  }

  if (validation_info & SPL_MODIFIED) {
    validation_info &= ~SPL_MODIFIED;
    errors.push_back(Error(offset, "error - %spl or %sp is changed"));
  }

  if (validation_info & BAD_CALL_ALIGNMENT) {
    validation_info &= ~BAD_CALL_ALIGNMENT;
    errors.push_back(Error(offset, "warning - bad call alignment"));
  }

  if (validation_info & BAD_JUMP_TARGET) {
    validation_info &= ~BAD_JUMP_TARGET;
    user_data.bad_jump_targets->insert(offset);
  }

  if (validation_info & VALIDATION_ERRORS_MASK) {
    errors.push_back(Error(offset, "<some other error>"));
  }

  return result;
}


Bool ProcessError(
    const uint8_t *begin, const uint8_t *end,
    uint32_t validation_info, void *user_data_ptr) {
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);
  UNREFERENCED_PARAMETER(validation_info);
  UNREFERENCED_PARAMETER(user_data_ptr);
  return FALSE;
}


typedef Bool ValidateChunkFunc(
    const uint8_t *data, size_t size,
    uint32_t options,
    const NaClCPUFeaturesX86 *cpu_features,
    ValidationCallbackFunc user_callback,
    void *callback_data);


bool Validate(
    const Segment &segment,
    ValidateChunkFunc validate_chunk,
    vector<Error> *errors) {

  errors->clear();

  if (segment.vaddr % kBundleSize != 0) {
    errors->push_back(Error(
        segment.vaddr,
        "Text segment offset in memory is not bundle-aligned."));
    return false;
  }

  if (segment.size % kBundleSize != 0) {
    char buf[100];
    SNPRINTF(buf, sizeof buf,
             "Text segment size (0x%"NACL_PRIx32") is not "
             "multiple of bundle size.",
             segment.size);
    errors->push_back(Error(segment.vaddr + segment.size, buf));
    return false;
  }

  vector<Jump> jumps;
  set<uint32_t> bad_jump_targets;

  UserData user_data(segment, errors, &jumps, &bad_jump_targets);

  // TODO(shcherbina): customize from command line

  // We collect all errors except bad jump targets, and we separately
  // memoize all direct jumps (or calls) and bad jump targets.
  Bool result = validate_chunk(
      segment.data, segment.size,
      CALL_USER_CALLBACK_ON_EACH_INSTRUCTION, &kFullCPUIDFeatures,
      ProcessInstruction, &user_data);

  // Report destinations of jumps that lead to bad targets.
  for (size_t i = 0; i < jumps.size(); i++) {
    const Jump &jump = jumps[i];
    if (bad_jump_targets.count(jump.to) > 0)
      errors->push_back(Error(jump.from, "bad jump target"));
  }

  // Run validator as it is run in loader, without callback on each instruction,
  // and ensure that results match. If ncval is guaranteed to return the same
  // result as actual validator, it can be reliably used as validator wrapper
  // for testing.
  CHECK(result == validate_chunk(
      segment.data, segment.size,
      0, &kFullCPUIDFeatures,
      ProcessError, NULL));

  return static_cast<bool>(result);
}


void Usage() {
  printf("Usage:\n");
  printf("    ncval <ELF file>\n");
  exit(1);
}


struct Options {
  const char *input_file;
};


void ParseOptions(size_t argc, const char * const *argv, Options *options) {
  if (argc != 2)
    Usage();

  options->input_file = argv[argc - 1];
}


int main(int argc, char **argv) {
  Options options;
  ParseOptions(argc, argv, &options);

  Image image;
  ReadImage(options.input_file, &image);

  Segment segment = GetElfTextSegment(image);

  vector<Error> errors;
  bool result = false;
  switch (segment.bitness) {
    case 32:
      result = Validate(segment, ValidateChunkIA32, &errors);
      break;
    case 64:
      result = Validate(segment, ValidateChunkAMD64, &errors);
      break;
    default:
      CHECK(false);
  }

  for (size_t i = 0; i < errors.size(); i++) {
    const Error &e = errors[i];
    printf("%8" NACL_PRIx32 ": %s\n", e.offset, e.message.c_str());
  }

  if (result) {
    printf("Valid.\n");
    return 0;
  } else {
    printf("Invalid.\n");
    return 1;
  }
}
