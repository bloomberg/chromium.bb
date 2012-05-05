/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf64.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

/* This is a copy of NaClLog_Function from shared/platform/nacl_log.c to avoid
 * linking in code in NaCl shared code in the unreviewed/Makefile and be able to
 *  use CHECK().

 * TODO(khim): remove the copy of NaClLog_Function implementation as soon as
 * unreviewed/Makefile is eliminated.
 */
void NaClLog_Function(int detail_level, char const  *fmt, ...) {
  va_list ap;

  UNREFERENCED_PARAMETER(detail_level);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  exit(1);
}

/* Emulate features of old validator to simplify testing */
NaClCPUFeaturesX86 old_validator_features = { { 1, 1 }, {
  1, /* NaClCPUFeature_3DNOW */  /* AMD-specific */
  0, /* NaClCPUFeature_AES */
  0, /* NaClCPUFeature_AVX */
  0, /* NaClCPUFeature_BMI1 */
  1, /* NaClCPUFeature_CLFLUSH */
  0, /* NaClCPUFeature_CLMUL */
  1, /* NaClCPUFeature_CMOV */
  1, /* NaClCPUFeature_CX16 */
  1, /* NaClCPUFeature_CX8 */
  1, /* NaClCPUFeature_E3DNOW */ /* AMD-specific */
  1, /* NaClCPUFeature_EMMX */   /* AMD-specific */
  0, /* NaClCPUFeature_F16C */
  0, /* NaClCPUFeature_FMA */
  0, /* NaClCPUFeature_FMA4 */ /* AMD-specific */
  1, /* NaClCPUFeature_FXSR */
  0, /* NaClCPUFeature_LAHF */
  0, /* NaClCPUFeature_LM */
  0, /* NaClCPUFeature_LWP */ /* AMD-specific */
  1, /* NaClCPUFeature_LZCNT */  /* AMD-specific */
  1, /* NaClCPUFeature_MMX */
  1, /* NaClCPUFeature_MON */
  1, /* NaClCPUFeature_MOVBE */
  1, /* NaClCPUFeature_OSXSAVE */
  1, /* NaClCPUFeature_POPCNT */
  0, /* NaClCPUFeature_PRE */ /* AMD-specific */
  1, /* NaClCPUFeature_SSE */
  1, /* NaClCPUFeature_SSE2 */
  1, /* NaClCPUFeature_SSE3 */
  1, /* NaClCPUFeature_SSE41 */
  1, /* NaClCPUFeature_SSE42 */
  1, /* NaClCPUFeature_SSE4A */  /* AMD-specific */
  1, /* NaClCPUFeature_SSSE3 */
  0, /* NaClCPUFeature_TBM */ /* AMD-specific */
  1, /* NaClCPUFeature_TSC */
  1, /* NaClCPUFeature_x87 */
  0  /* NaClCPUFeature_XOP */ /* AMD-specific */
} };

/* Emulate features of old validator to simplify testing */
NaClCPUFeaturesX86 full_validator_features = { { 1, 1 }, {
  1, /* NaClCPUFeature_3DNOW */  /* AMD-specific */
  1, /* NaClCPUFeature_AES */
  1, /* NaClCPUFeature_AVX */
  1, /* NaClCPUFeature_BMI1 */
  1, /* NaClCPUFeature_CLFLUSH */
  1, /* NaClCPUFeature_CLMUL */
  1, /* NaClCPUFeature_CMOV */
  1, /* NaClCPUFeature_CX16 */
  1, /* NaClCPUFeature_CX8 */
  1, /* NaClCPUFeature_E3DNOW */ /* AMD-specific */
  1, /* NaClCPUFeature_EMMX */   /* AMD-specific */
  1, /* NaClCPUFeature_F16C */
  1, /* NaClCPUFeature_FMA */
  1, /* NaClCPUFeature_FMA4 */ /* AMD-specific */
  1, /* NaClCPUFeature_FXSR */
  1, /* NaClCPUFeature_LAHF */
  1, /* NaClCPUFeature_LM */
  1, /* NaClCPUFeature_LWP */ /* AMD-specific */
  1, /* NaClCPUFeature_LZCNT */  /* AMD-specific */
  1, /* NaClCPUFeature_MMX */
  1, /* NaClCPUFeature_MON */
  1, /* NaClCPUFeature_MOVBE */
  1, /* NaClCPUFeature_OSXSAVE */
  1, /* NaClCPUFeature_POPCNT */
  1, /* NaClCPUFeature_PRE */ /* AMD-specific */
  1, /* NaClCPUFeature_SSE */
  1, /* NaClCPUFeature_SSE2 */
  1, /* NaClCPUFeature_SSE3 */
  1, /* NaClCPUFeature_SSE41 */
  1, /* NaClCPUFeature_SSE42 */
  1, /* NaClCPUFeature_SSE4A */  /* AMD-specific */
  1, /* NaClCPUFeature_SSSE3 */
  1, /* NaClCPUFeature_TBM */ /* AMD-specific */
  1, /* NaClCPUFeature_TSC */
  1, /* NaClCPUFeature_x87 */
  1  /* NaClCPUFeature_XOP */ /* AMD-specific */
} };

static void CheckBounds(unsigned char *data, size_t data_size,
                        void *ptr, size_t inside_size) {
  CHECK(data <= (unsigned char *) ptr);
  CHECK((unsigned char *) ptr + inside_size <= data + data_size);
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

void ProcessError (const uint8_t *ptr, void *userdata) {
  printf("offset 0x%"NACL_PRIxS": DFA error in validator\n",
                  (size_t)(ptr - (((struct ValidateState *)userdata)->offset)));
}

int ValidateFile(const char *filename, int repeat_count,
                 const NaClCPUFeaturesX86 *cpu_features) {
  size_t data_size;
  uint8_t *data;
  int count;

  ReadImage(filename, &data, &data_size);

  if (data[4] == 1) {
    for (count = 0; count < repeat_count; ++count) {
      Elf32_Ehdr *header;
      int index;

      header = (Elf32_Ehdr *) data;
      CheckBounds(data, data_size, header, sizeof(*header));
      assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

      for (index = 0; index < header->e_shnum; ++index) {
        Elf32_Shdr *section = (Elf32_Shdr *) (data + header->e_shoff +
                                                   header->e_shentsize * index);
        CheckBounds(data, data_size, section, sizeof(*section));

        if ((section->sh_flags & SHF_EXECINSTR) != 0) {
          struct ValidateState state;
          int res;

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
                                  cpu_features, ProcessError, &state);
          if (res != 0) {
            return res;
          }
        }
      }
    }
  } else if (data[4] == 2) {
    for (count = 0; count < repeat_count; ++count) {
      Elf64_Ehdr *header;
      int index;

      header = (Elf64_Ehdr *) data;
      CheckBounds(data, data_size, header, sizeof(*header));
      assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

      for (index = 0; index < header->e_shnum; ++index) {
        Elf64_Shdr *section = (Elf64_Shdr *) (data + header->e_shoff +
                                                   header->e_shentsize * index);
        CheckBounds(data, data_size, section, sizeof(*section));

        if ((section->sh_flags & SHF_EXECINSTR) != 0) {
          struct ValidateState state;
          int res;

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
                                   cpu_features, ProcessError, &state);
          if (res != 0) {
            return res;
          }
        }
      }
    }
  } else {
    printf("Unknown ELF class: %s\n", filename);
    exit(1);
  }
  return 0;
}

int main(int argc, char **argv) {
  int index, initial_index = 1, repeat_count = 1;
  int use_old_features = 0;
  if (argc == 1) {
    printf("%s: no input files\n", argv[0]);
  }
  for (;;) {
    if (!strcmp(argv[initial_index], "--repeat")) {
      repeat_count = atoi(argv[initial_index + 1]);
      initial_index += 2;
      if (initial_index < argc)
        continue;
    }
    if (!strcmp(argv[initial_index], "--compatible")) {
      use_old_features = 1;
      ++initial_index;
      if (initial_index < argc)
        continue;
    }
    break;
  }
  for (index = initial_index; index < argc; ++index) {
    const char *filename = argv[index];
    int rc = ValidateFile(filename, repeat_count,
                          use_old_features ? &old_validator_features :
                                             &full_validator_features);
    if (rc != 0) {
      printf("file '%s' can not be fully validated\n", filename);
      return 1;
    }
  }
  return 0;
}
