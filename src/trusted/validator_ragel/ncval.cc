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
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

using std::set;
using std::string;
using std::vector;


// Hypothetically reading whole ELF file to memory can cause problems with huge
// amounts of debug info, but unless it actually happens this approach is used
// for simplicity.
void ReadImage(const char *filename, vector<uint8_t> *data) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", filename);
    exit(1);
  }

  fseek(fp, 0, SEEK_END);
  size_t file_size = ftell(fp);

  data->resize(file_size);

  fseek(fp, 0, SEEK_SET);
  size_t got = fread(&(*data)[0], 1, file_size, fp);
  if (got != file_size) {
    fprintf(stderr, "Unable to read data from input file: %s\n",
            filename);
    exit(1);
  }
  fclose(fp);
}


struct Segment {
  const uint8_t *data;
  uint32_t size;
  uint32_t vaddr;
};


template<typename ElfEhdrType, typename ElfPhdrType>
Segment FindTextSegment(const vector<uint8_t> &data) {
  Segment segment = {NULL, 0, 0};  // only to suppress 'uninitialized' warning
  bool found = false;

  const ElfEhdrType &header = *reinterpret_cast<const ElfEhdrType *>(&data[0]);
  CHECK(sizeof(header) <= data.size());
  CHECK(memcmp(header.e_ident, ELFMAG, SELFMAG) == 0);

  for (uint64_t index = 0; index < header.e_phnum; ++index) {
    uint64_t phdr_offset = header.e_phoff + header.e_phentsize * index;
    // static_cast to silence msvc on 32-bit platform
    const ElfPhdrType &phdr = *reinterpret_cast<const ElfPhdrType *>(
        &data[static_cast<size_t>(phdr_offset)]);

    // TODO(shcherbina): size of other loadable segments
    if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X)) {
      if (found) {
        printf("More than one text segment.\n");
        exit(1);
      }

      if (phdr.p_flags != (PF_R | PF_X)) {
        // Cast to support 64-bit ELF.
        printf("Text segment is expected to have flags PF_R | PF_X "
               "(has 0x%" NACL_PRIx64 " instead).\n",
               static_cast<uint64_t>(phdr.p_flags));
        exit(1);
      }

      CHECK(phdr.p_filesz <= phdr.p_memsz);
      if (phdr.p_filesz < phdr.p_memsz) {
        printf("File image is smaller than memory image size.\n");
        exit(1);
      }

      // TODO(shcherbina): find or introduce proper constant.
      if (phdr.p_filesz > 256 << 20) {
        printf("Test segment is too large.");
        exit(1);
      }

      if (phdr.p_vaddr > UINT32_MAX - phdr.p_filesz) {
        printf("Text segment does not fit in 4GB.");
        exit(1);
      }

      segment.data = &data[static_cast<size_t>(phdr.p_offset)];
      segment.size = static_cast<uint32_t>(phdr.p_filesz);
      segment.vaddr = static_cast<uint32_t>(phdr.p_vaddr);
      found = true;
    }
  }
  if (!found) {
    printf("Text segment not found.\n");
    exit(1);
  }
  return segment;
}


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
    enum ValidationOptions options,
    const NaClCPUFeaturesX86 *cpu_features,
    ValidationCallbackFunc user_callback,
    void *callback_data);


bool Validate(
    const Segment &segment,
    ValidateChunkFunc validate_chunk,
    vector<Error> *errors) {

  errors->clear();

  vector<Jump> jumps;
  set<uint32_t> bad_jump_targets;

  UserData user_data(segment, errors, &jumps, &bad_jump_targets);

  // TODO(shcherbina): customize from command line
  const NaClCPUFeaturesX86 *cpu_features = &kFullCPUIDFeatures;

  // We collect all errors except bad jump targets, and we separately
  // memoize all direct jumps (or calls) and bad jump targets.
  Bool result = validate_chunk(
      segment.data, segment.size,
      CALL_USER_CALLBACK_ON_EACH_INSTRUCTION, cpu_features,
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
      static_cast<ValidationOptions>(0), cpu_features,
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
  printf("Validating %s ...\n", options.input_file);

  vector<uint8_t> data;
  ReadImage(options.input_file, &data);

  const Elf32_Ehdr &header = *reinterpret_cast<const Elf32_Ehdr *>(&data[0]);
  if (data.size() < sizeof(header) ||
      memcmp(header.e_ident, ELFMAG, SELFMAG) != 0) {
    printf("Not an ELF file.\n");
    exit(1);
  }

  Segment segment;
  switch (header.e_ident[EI_CLASS]) {
    case ELFCLASS32:
      segment = FindTextSegment<Elf32_Ehdr, Elf32_Phdr>(data);
      break;
    case ELFCLASS64:
      segment = FindTextSegment<Elf64_Ehdr, Elf64_Phdr>(data);
      break;
    default:
      printf("Invalid ELF class %d.\n", header.e_ident[EI_CLASS]);
      exit(1);
  }

  if (segment.size % kBundleSize != 0) {
    printf("Text segment size (0x%"NACL_PRIx32") is not "
           "multiple of bundle size.\n",
           segment.size);
    exit(1);
  }
  if (segment.vaddr % kBundleSize != 0) {
    printf("Text segment offset in memory (0x%"NACL_PRIx32") "
           "is not bundle-aligned.\n",
           segment.vaddr);
    exit(1);
  }

  vector<Error> errors;
  bool result = false;
  switch (header.e_machine) {
    case EM_386:
      result = Validate(segment, ValidateChunkIA32, &errors);
      break;
    case EM_X86_64:
      result = Validate(segment, ValidateChunkAMD64, &errors);
      break;
    default:
      printf("Unsupported e_machine %"NACL_PRIu16".\n", header.e_machine);
      exit(1);
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
