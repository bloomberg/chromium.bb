/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf64.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator_internal.h"

/* This is a copy of NaClLog from shared/platform/nacl_log.c to avoid
 * linking in code in NaCl shared code in the unreviewed/Makefile and be able to
 *  use CHECK().

 * TODO(khim): remove the copy of NaClLog implementation as soon as
 * unreviewed/Makefile is eliminated.
 */
void NaClLog(int detail_level, char const  *fmt, ...) {
  va_list ap;

  UNREFERENCED_PARAMETER(detail_level);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  exit(1);
}

static void CheckBounds(const unsigned char *data, size_t data_size,
                        const void *ptr, size_t inside_size) {
  CHECK(data <= (const unsigned char *) ptr);
  CHECK((const unsigned char *) ptr + inside_size <= data + data_size);
}

void ReadImage(const char *filename, uint8_t **result, size_t *result_size) {
  FILE *fp;
  uint8_t *data;
  size_t file_size;
  size_t got;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", filename);
    exit(1);
  }
  /* Find the file size. */
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  data = malloc(file_size);
  if (data == NULL) {
    fprintf(stderr, "Unable to create memory image of input file: %s\n",
            filename);
    exit(1);
  }
  fseek(fp, 0, SEEK_SET);
  got = fread(data, 1, file_size, fp);
  if (got != file_size) {
    fprintf(stderr, "Unable to read data from input file: %s\n",
            filename);
    exit(1);
  }
  fclose(fp);

  *result = data;
  *result_size = file_size;
}

struct ValidateState {
  uint8_t width;
  const uint8_t *offset;
};

Bool ProcessError(const uint8_t *begin, const uint8_t *end,
                  uint32_t error_code, void *userdata) {
  size_t offset = begin - (((struct ValidateState *)userdata)->offset);
  UNREFERENCED_PARAMETER(end);

  if (error_code & UNRECOGNIZED_INSTRUCTION)
    printf("offset 0x%"NACL_PRIxS": unrecognized instruction\n", offset);
  if (error_code & DIRECT_JUMP_OUT_OF_RANGE)
    printf("offset 0x%"NACL_PRIxS": direct jump out of range\n", offset);
  if (error_code & CPUID_UNSUPPORTED_INSTRUCTION)
    printf("offset 0x%"NACL_PRIxS": required CPU feature not found\n", offset);
  if (error_code & FORBIDDEN_BASE_REGISTER)
    printf("offset 0x%"NACL_PRIxS": improper memory address - bad base\n",
                                                                        offset);
  if (error_code & UNRESTRICTED_INDEX_REGISTER)
    printf("offset 0x%"NACL_PRIxS": improper memory address - bad index\n",
                                                                        offset);
  if ((error_code & BAD_RSP_RBP_PROCESSING_MASK) == RESTRICTED_RBP_UNPROCESSED)
    printf("offset 0x%"NACL_PRIxS": improper %%rbp sandboxing\n", offset);
  if ((error_code & BAD_RSP_RBP_PROCESSING_MASK) == UNRESTRICTED_RBP_PROCESSED)
    printf("offset 0x%"NACL_PRIxS": improper %%rbp sandboxing\n", offset);
  if ((error_code & BAD_RSP_RBP_PROCESSING_MASK) == RESTRICTED_RSP_UNPROCESSED)
    printf("offset 0x%"NACL_PRIxS": improper %%rsp sandboxing\n", offset);
  if ((error_code & BAD_RSP_RBP_PROCESSING_MASK) == UNRESTRICTED_RSP_PROCESSED)
    printf("offset 0x%"NACL_PRIxS": improper %%rsp sandboxing\n", offset);
  if (error_code & R15_MODIFIED)
    printf("offset 0x%"NACL_PRIxS": error - %%r15 is changed\n", offset);
  if (error_code & BPL_MODIFIED)
    printf("offset 0x%"NACL_PRIxS": error - %%bpl or %%bp is changed\n",
                                                                        offset);
  if (error_code & SPL_MODIFIED)
    printf("offset 0x%"NACL_PRIxS": error - %%spl or %%sp is changed\n",
                                                                        offset);
  if (error_code & BAD_JUMP_TARGET)
    printf("bad jump to around 0x%"NACL_PRIxS"\n", offset);
  if (error_code & (VALIDATION_ERRORS_MASK | BAD_JUMP_TARGET))
    return FALSE;
  else
    return TRUE;
}

Bool ProcessErrorOrWarning(const uint8_t *begin, const uint8_t *end,
                           uint32_t error_code, void *userdata) {
  size_t offset = begin - (((struct ValidateState *)userdata)->offset);
  UNREFERENCED_PARAMETER(end);

  if (error_code & BAD_CALL_ALIGNMENT)
    printf("offset 0x%"NACL_PRIxS": warning - bad call alignment\n", offset);
  return ProcessError(begin, end, error_code, userdata);
}

Bool ValidateElf32(const uint8_t *data, size_t data_size,
                   Bool warnings,
                   enum ValidationOptions options,
                   const NaClCPUFeaturesX86 *cpu_features) {
  Elf32_Ehdr *header;
  int index;

  header = (Elf32_Ehdr *) data;
  CheckBounds(data, data_size, header, sizeof *header);
  assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

  for (index = 0; index < header->e_shnum; ++index) {
    Elf32_Shdr *section = (Elf32_Shdr *) (data + header->e_shoff +
                                               header->e_shentsize * index);
    CheckBounds(data, data_size, section, sizeof *section);

    if ((section->sh_flags & SHF_EXECINSTR) != 0) {
      struct ValidateState state;
      Bool res;

      state.offset = data + section->sh_offset - section->sh_addr;
      if (section->sh_size <= 0xfff) {
        state.width = 4;
      } else if (section->sh_size <= 0xfffffff) {
        state.width = 8;
      } else {
        state.width = 12;
      }
      CheckBounds(data, data_size,
                  data + section->sh_offset, section->sh_size);
      res = ValidateChunkIA32(data + section->sh_offset, section->sh_size,
                              options, cpu_features,
                              warnings ? ProcessErrorOrWarning : ProcessError,
                              &state);
      if (!res)
        return res;
    }
  }
  return TRUE;
}

Bool ValidateElf64(const uint8_t *data, size_t data_size,
                   Bool warnings,
                   enum ValidationOptions options,
                   const NaClCPUFeaturesX86 *cpu_features) {
  Elf64_Ehdr *header;
  int index;

  header = (Elf64_Ehdr *) data;
  CheckBounds(data, data_size, header, sizeof *header);
  assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

  for (index = 0; index < header->e_shnum; ++index) {
    Elf64_Shdr *section = (Elf64_Shdr *) (data + header->e_shoff +
                                               header->e_shentsize * index);
    CheckBounds(data, data_size, section, sizeof *section);

    if ((section->sh_flags & SHF_EXECINSTR) != 0) {
      struct ValidateState state;
      Bool res;

      state.offset = data + section->sh_offset - section->sh_addr;
      if (section->sh_size <= 0xfff) {
        state.width = 4;
      } else if (section->sh_size <= 0xfffffff) {
        state.width = 8;
      } else if (section->sh_size <= 0xfffffffffffLL) {
        state.width = 12;
      } else {
        state.width = 16;
      }
      CheckBounds(data, data_size,
                  data + section->sh_offset, (size_t)section->sh_size);
      res = ValidateChunkAMD64(data + section->sh_offset,
                               (size_t)section->sh_size,
                               options, cpu_features,
                               warnings ? ProcessErrorOrWarning : ProcessError,
                               &state);
      if (!res)
        return res;
    }
  }
  return TRUE;
}

Bool ValidateElf(const char *filename,
                 const uint8_t *data, size_t data_size,
                 Bool warnings,
                 enum ValidationOptions options,
                 const NaClCPUFeaturesX86 *cpu_features) {
  if (data[4] == 1) {
    return ValidateElf32(data, data_size, warnings, options, cpu_features);
  } else if (data[4] == 2) {
    return ValidateElf64(data, data_size, warnings, options, cpu_features);
  } else {
    printf("Unknown ELF class: %s\n", filename);
    exit(1);
  }
}

void ProcessFile(const char *filename,
                 int repeat_count,
                 int raw_bitness,
                 Bool warnings,
                 enum ValidationOptions options,
                 const NaClCPUFeaturesX86 *cpu_features) {
  size_t data_size;
  uint8_t *data;
  int count;

  ReadImage(filename, &data, &data_size);

  for (count = 0; count < repeat_count; ++count) {
    Bool rc = FALSE;
    if (raw_bitness == 0)
      rc = ValidateElf(filename,
                       data, data_size,
                       warnings,
                       options,
                       cpu_features);
    else if (raw_bitness == 32) {
      struct ValidateState state;
      state.offset = data;
      CHECK(data_size % kBundleSize == 0);
      rc = ValidateChunkIA32(data, data_size,
                             options, cpu_features,
                             warnings ? ProcessErrorOrWarning : ProcessError,
                             &state);
    }
    else if (raw_bitness == 64) {
      struct ValidateState state;
      state.offset = data;
      CHECK(data_size % kBundleSize == 0);
      rc = ValidateChunkAMD64(data, data_size,
                              options, cpu_features,
                              warnings ? ProcessErrorOrWarning : ProcessError,
                              &state);
    }
    if (!rc) {
      printf("file '%s' can not be fully validated\n", filename);
      exit(1);
    }
  }
}

int main(int argc, char **argv) {
  int index, initial_index = 1, repeat_count = 1;
  const NaClCPUFeaturesX86 *cpu_features = &kFullCPUIDFeatures;
  int raw_bitness = 0;
  Bool warnings = FALSE;
  enum ValidationOptions options = 0;

  if (argc == 1) {
    printf("%s: no input files\n", argv[0]);
    return 2;
  }
  while (initial_index < argc) {
    char *arg = argv[initial_index];
    if (!strcmp(arg, "--compatible")) {
      cpu_features = &kValidatorCPUIDFeatures;
      initial_index++;
    } else if (!strcmp(argv[initial_index], "--warnings")) {
      warnings = TRUE;
      options |= CALL_USER_CALLBACK_ON_EACH_INSTRUCTION;
      initial_index++;
    } else if (!strcmp(arg, "--nobundles")) {
      options |= PROCESS_CHUNK_AS_A_CONTIGUOUS_STREAM;
      initial_index++;
    } else if (!strcmp(arg, "--repeat")) {
      if (initial_index+1 >= argc) {
        printf("%s: no integer after --repeat\n", argv[0]);
        return 2;
      }
      repeat_count = atoi(argv[initial_index + 1]);
      initial_index += 2;
    } else if (!strcmp(argv[initial_index], "--raw32")) {
      raw_bitness = 32;
      initial_index++;
    } else if (!strcmp(argv[initial_index], "--raw64")) {
      raw_bitness = 64;
      initial_index++;
    } else {
      break;
    }
  }
  for (index = initial_index; index < argc; ++index) {
    const char *filename = argv[index];
    ProcessFile(filename, repeat_count, raw_bitness, warnings, options,
                cpu_features);
  }
  return 0;
}
