/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "native_client/src/include/elf.h"
#include "native_client/src/include/elf_constants.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/elf_load.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"


Bool ProcessError(
    const uint8_t *begin, const uint8_t *end,
    uint32_t validation_info, void *user_data_ptr) {
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);
  UNREFERENCED_PARAMETER(user_data_ptr);
  if (validation_info & (VALIDATION_ERRORS_MASK | BAD_JUMP_TARGET))
    return FALSE;
  else
    return TRUE;
}


int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage:\n");
    printf("    validator_benchmark <nexe> <number of repetitions>\n");
    exit(1);
  }
  const char *input_file = argv[1];
  int repetitions = atoi(argv[2]);
  CHECK(repetitions > 0);

  printf("Validating %s %d times ...\n", input_file, repetitions);

  Image image;
  ReadImage(input_file, &image);

  const Elf32_Ehdr &header = *reinterpret_cast<const Elf32_Ehdr *>(&image[0]);
  if (image.size() < sizeof(header) ||
      memcmp(header.e_ident, ELFMAG, SELFMAG) != 0) {
    printf("Not an ELF file.\n");
    exit(1);
  }

  Segment segment;
  switch (header.e_ident[EI_CLASS]) {
    case ELFCLASS32:
      segment = FindTextSegment<Elf32_Ehdr, Elf32_Phdr>(image);
      break;
    case ELFCLASS64:
      segment = FindTextSegment<Elf64_Ehdr, Elf64_Phdr>(image);
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

  Bool result = FALSE;

  const NaClCPUFeaturesX86 *cpu_features = &kFullCPUIDFeatures;

  clock_t start = clock();
  for (int i = 0; i < repetitions; i++) {
    switch (header.e_machine) {
      case EM_386:
        result = ValidateChunkIA32(
            segment.data, segment.size,
            0, cpu_features,
            ProcessError, NULL);
        break;
      case EM_X86_64:
        result = ValidateChunkAMD64(
            segment.data, segment.size,
            0, cpu_features,
            ProcessError, NULL);
        break;
      default:
        printf("Unsupported e_machine %"NACL_PRIu16".\n", header.e_machine);
        exit(1);
    }
  }

  if (result)
    printf("Valid.\n");
  else
    printf("Invalid.\n");

  float seconds = (float)(clock() - start) / CLOCKS_PER_SEC;
  printf("It took %.3fs", seconds);

  if (seconds > 1e-6)
    printf(" (%.3f MB/s)", segment.size / seconds * repetitions / (1<<20));

  printf("\n");

  return result ? 0 : 1;
}
