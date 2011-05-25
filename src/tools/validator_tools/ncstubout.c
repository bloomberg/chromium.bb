/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"

#if NACL_TARGET_SUBARCH == 32

# include "native_client/src/trusted/validator_x86/ncdecode.h"
# include "native_client/src/trusted/validator_x86/ncvalidate.h"

static Bool FixUpSection(uintptr_t load_address,
                         unsigned char *code,
                         size_t code_size) {
  int return_code;
  struct NCValidatorState *vstate;
  int bundle_size = 32;
  CPUFeatures features;

  /* Start by stubbing out the code. */
  vstate = NCValidateInit(load_address, load_address + code_size, bundle_size);
  if (vstate == NULL) {
    fprintf(stderr, "Unable to stubout code, not enough memory\n");
    return FALSE;
  }

  NCValidateSetStubOutMode(vstate, 1);

  /*
   * We should not stub out any instructions based on the features
   * of the CPU we are executing on now.
   */
  NaClSetAllCPUFeatures(&features);
  NCValidatorStateSetCPUFeatures(vstate, &features);
  NCValidateSegment(code, load_address, code_size, vstate);
  NCValidateFinish(vstate);
  NCValidateFreeState(&vstate);

  /* Now run the validator again, so that we report any errors
   * that were not fixed by stubbing out. This is done so that
   * the user knows that stubout doesn't fix all errors.
   */
  vstate = NCValidateInit(load_address, load_address + code_size, bundle_size);
  if (vstate == NULL) {
    fprintf(stderr, "Unable to stubout code, not enough memory\n");
    return FALSE;
  }
  NCValidatorStateSetCPUFeatures(vstate, &features);
  NCValidateSegment(code, load_address, code_size, vstate);
  return_code = NCValidateFinish(vstate);
  NCValidateFreeState(&vstate);
  return return_code ? FALSE : TRUE;
}

#else

# include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
# include "native_client/src/trusted/validator_x86/ncval_driver.h"

static Bool FixUpSection(uintptr_t load_address,
                         unsigned char *code,
                         size_t code_size) {
  struct NaClValidatorState *vstate;
  int bundle_size = 32;
  CPUFeatures features;
  Bool return_code;

  /* Start by stubbing out the code. */
  vstate = NaClValidatorStateCreate(load_address, code_size, bundle_size,
                                    RegR15);
  if (vstate == NULL) {
    fprintf(stderr, "Unable to stubout code, not enough memory\n");
    return FALSE;
  }
  NaClValidatorStateSetDoStubOut(vstate, TRUE);

  /*
   * We should not stub out any instructions based on the features
   * of the CPU we are executing on now.
   */
  NaClSetAllCPUFeatures(&features);
  NaClValidatorStateSetCPUFeatures(vstate, &features);
  NaClValidatorStateSetErrorReporter(vstate, &kNaClVerboseErrorReporter);
  NaClValidateSegment(code, load_address, code_size, vstate);
  return_code = NaClValidatesOk(vstate);
  NaClValidatorStateDestroy(vstate);

  /* Now run the validator again, so that we report any errors
   * that were not fixed by stubbing out. This is done so that
   * the user knows that stubout doesn't fix all errors.
   */
  vstate = NaClValidatorStateCreate(load_address, code_size, bundle_size,
                                    RegR15);
  if (vstate == NULL) {
    fprintf(stderr, "Unable to stubout code, not enough memory\n");
    return FALSE;
  }
  NaClValidatorStateSetCPUFeatures(vstate, &features);
  NaClValidateSegment(code, load_address, code_size, vstate);
  return_code = NaClValidatesOk(vstate);
  NaClValidatorStateDestroy(vstate);
  return return_code;
}

#endif

static void CheckBounds(unsigned char *data, size_t data_size,
                        void *ptr, size_t inside_size) {
  CHECK(data <= (unsigned char *) ptr);
  CHECK((unsigned char *) ptr + inside_size <= data + data_size);
}

static Bool FixUpELF(unsigned char *data, size_t data_size) {
  Elf_Ehdr *header;
  int index;
  Bool fixed = TRUE;  /* until proven otherwise. */

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
      if (!FixUpSection(section->sh_addr,
                        data + section->sh_offset, section->sh_size)) {
        fixed = FALSE;
      }
    }
  }
  return fixed;
}

static Bool FixUpELFFile(const char *input_file, const char *output_file) {
  FILE *fp;
  size_t file_size;
  unsigned char *data;
  size_t got;
  size_t written;

  /* Read whole ELF file and write it back with modifications. */
  fp = fopen(input_file, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", input_file);
    return FALSE;
  }
  /* Find the file size. */
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  data = malloc(file_size);
  if (data == NULL) {
    fprintf(stderr, "Unable to create memory imate of input file: %s\n",
            input_file);
    return FALSE;
  }
  fseek(fp, 0, SEEK_SET);
  got = fread(data, 1, file_size, fp);
  if (got != file_size) {
    fprintf(stderr, "Unable to read data from input file: %s\n",
            input_file);
    return FALSE;
  }
  fclose(fp);

  if (!FixUpELF(data, file_size)) return FALSE;

  fp = fopen(output_file, "wb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open output file: %s\n", output_file);
    return FALSE;
  }
  written = fwrite(data, 1, file_size, fp);
  if (written != file_size) {
    fprintf(stderr, "Unable to write data to output file: %s\n",
            output_file);
    return FALSE;
  }
  fclose(fp);
  return TRUE;
}

int main(int argc, const char *argv[]) {
  /* Be sure to redirect validator error messages to stderr. */
  struct GioFile err;
  GioFileRefCtor(&err, stderr);
  NaClLogPreInitSetGio((struct Gio*) &err);
  NaClLogModuleInit();
  if (argc != 4 || strcmp(argv[2], "-o") != 0) {
    fprintf(stderr, "Usage: %s <input-file> -o <output-file>\n\n", argv[0]);
    fprintf(stderr,
            "This tool rewrites ELF objects to replace instructions that are\n"
            "rejected by the NaCl validator with safe HLT instructions.\n");
    GioFileDtor((struct Gio*) &err);
    return 1;
  }
  GioFileDtor((struct Gio*) &err);
  return FixUpELFFile(argv[1], argv[3]) ? 0 : 1;
}
