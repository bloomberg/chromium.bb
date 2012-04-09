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
 *unreviewed/Makefile is eliminated.
 */
void NaClLog_Function(int detail_level, char const  *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  exit(1);
  NaClLogV_mu(detail_level, fmt, ap);
  va_end(ap);
  NaClLogUnlock();
}

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
                            ptr - (((struct ValidateState *)userdata)->offset));
}

int ValidateFile(const char *filename, int repeat_count) {
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
          res = ValidateChunkIA32(data + section->sh_offset,
                                        section->sh_size, ProcessError, &state);
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
                      data + section->sh_offset, section->sh_size);
          res = ValidateChunkAMD64(data + section->sh_offset,
                                        section->sh_size, ProcessError, &state);
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
  if (argc == 1) {
    printf("%s: no input files\n", argv[0]);
  }
  if (!strcmp(argv[1],"--repeat"))
    repeat_count = atoi(argv[2]),
    initial_index += 2;
  for (index = initial_index; index < argc; ++index) {
    const char *filename = argv[index];
    int rc = ValidateFile(filename, repeat_count);
    if (rc != 0) {
      printf("file '%s' can not be fully validated\n", filename);
      return 1;
    }
  }
  return 0;
}
