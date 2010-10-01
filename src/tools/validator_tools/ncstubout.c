/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * This tool rewrites ELF files to replace instructions that will be
 * rejected by the validator with safe HLT instructions.  This is
 * useful if you have a large library in which many functions do not
 * validate but are not immediately required to work.  Replacing the
 * forbidden instructions with HLTs makes it easier to find the
 * instructions that are needed first, and fix and test them.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/include/elf.h"
#include "native_client/src/shared/platform/nacl_check.h"


#if NACL_TARGET_SUBARCH == 32

# include "native_client/src/trusted/validator_x86/ncdecode.h"
# include "native_client/src/trusted/validator_x86/ncvalidate.h"
# include "native_client/src/trusted/validator_x86/ncvalidate_internaltypes.h"

static void FixUpSection(uintptr_t load_address,
                         unsigned char *code,
                         size_t code_size) {
  struct NCValidatorState *vstate;
  int bundle_size = 32;
  vstate = NCValidateInit(load_address, load_address + code_size, bundle_size);
  CHECK(vstate != NULL);
  vstate->do_stub_out = 1;

  /*
   * We should not stub out any instructions based on the features
   * of the CPU we are executing on now.
   */
  memset(&vstate->cpufeatures, 0xff, sizeof(vstate->cpufeatures));

  NCDecodeSegment(code, load_address, code_size, vstate);
  /*
   * We do not need to call NCValidateFinish() because it is
   * normal for validation to fail.
   */
  NCValidateFreeState(&vstate);
}

#else

# include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

static void FixUpSection(uintptr_t load_address,
                         unsigned char *code,
                         size_t code_size) {
  struct NaClValidatorState *vstate;
  int bundle_size = 32;
  vstate = NaClValidatorStateCreate(load_address, code_size, bundle_size,
                                    RegR15);
  CHECK(vstate != NULL);
  NaClValidatorStateSetDoStubOut(vstate, TRUE);
  NaClValidateSegment(code, load_address, code_size, vstate);
  NaClValidatorStateDestroy(vstate);
}

#endif

static void CheckBounds(unsigned char *data, size_t data_size,
                        void *ptr, size_t inside_size) {
  CHECK(data <= (unsigned char *) ptr);
  CHECK((unsigned char *) ptr + inside_size <= data + data_size);
}

static void FixUpELF(unsigned char *data, size_t data_size) {
  Elf_Ehdr *header;
  int index;

  header = (Elf_Ehdr *) data;
  CheckBounds(data, data_size, header, sizeof(*header));
  CHECK(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

  for (index = 0; index < header->e_shnum; index++) {
    Elf_Shdr *section = (Elf_Shdr *) (data + header->e_shoff +
                                      header->e_shentsize * index);
    CheckBounds(data, data_size, section, sizeof(*section));

    if ((section->sh_flags & SHF_EXECINSTR) != 0) {
      CheckBounds(data, data_size,
                  data + section->sh_offset, section->sh_size);
      FixUpSection(section->sh_addr,
                   data + section->sh_offset, section->sh_size);
    }
  }
}

static void FixUpELFFile(const char *input_file, const char *output_file) {
  FILE *fp;
  size_t file_size;
  unsigned char *data;
  size_t got;
  size_t written;

  /* Read whole ELF file and write it back with modifications. */
  fp = fopen(input_file, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", input_file);
    exit(1);
  }
  /* Find the file size. */
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  data = malloc(file_size);
  CHECK(data != NULL);
  fseek(fp, 0, SEEK_SET);
  got = fread(data, 1, file_size, fp);
  CHECK(got == file_size);
  fclose(fp);

  FixUpELF(data, file_size);

  fp = fopen(output_file, "wb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open output file: %s\n", output_file);
    exit(1);
  }
  written = fwrite(data, 1, file_size, fp);
  CHECK(written == file_size);
  fclose(fp);
}

int main(int argc, const char *argv[]) {
  NaClLogModuleInit();
  if (argc != 4 || strcmp(argv[2], "-o") != 0) {
    fprintf(stderr, "Usage: %s <input-file> -o <output-file>\n\n", argv[0]);
    fprintf(stderr,
            "This tool rewrites ELF objects to replace instructions that are\n"
            "rejected by the NaCl validator with safe HLT instructions.\n");
    return 1;
  }
  FixUpELFFile(argv[1], argv[3]);
  return 0;
}
