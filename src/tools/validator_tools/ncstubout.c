/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
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
#include "native_client/src/trusted/validator/ncvalidate.h"

static Bool FixUpSectionCheckStatus(NaClValidationStatus status) {
  switch (status) {
    case NaClValidationSucceeded:
      return TRUE;
    default:
    case NaClValidationFailed:
      fprintf(stderr, "Errors still exist after attempting to stubout code\n");
      return FALSE;
    case NaClValidationFailedOutOfMemory:
      fprintf(stderr, "Unable to stubout code, not enough memory\n");
      return FALSE;
    case NaClValidationFailedNotImplemented:
      fprintf(stderr, "Unable to stubout code, not implemented\n");
      return FALSE;
    case NaClValidationFailedCpuNotSupported:
      /* This shouldn't happen, but if it does, report the problem. */
      fprintf(stderr, "Unable to stubout code, cpu not supported\n");
      return FALSE;
    case NaClValidationFailedSegmentationIssue:
      fprintf(stderr, "Unable to stubout code, segmentation issues found\n");
      return FALSE;
  }
  return FALSE;
}

static Bool FixUpSection(uintptr_t load_address,
                         unsigned char *code,
                         size_t code_size) {
  Bool result;
  NaClValidationStatus status;
  const struct NaClValidatorInterface *validator = NaClCreateValidator();
  NaClCPUFeatures *cpu_features = malloc(validator->CPUFeatureSize);
  if (cpu_features == NULL) {
    fprintf(stderr, "Unable to create memory for CPU features\n");
    return FALSE;
  }
  /* Pretend that the CPU supports every feature so that we will only stub out
   * instructions that NaCl will never allow under any condition.
   */
  validator->SetAllCPUFeatures(cpu_features);

  status = validator->Validate(
      load_address, code, code_size,
      /* stubout_mode= */ TRUE,
      /* readonly_text= */ FALSE,
      cpu_features,
      /* metadata= */ NULL,
      /* cache= */ NULL);
  if (status == NaClValidationSucceeded) {
    /* Now run the validator again, so that we report any errors
     * that were not fixed by stubbing out. This is done so that
     * the user knows that stubout doesn't fix all errors.
     */
    status = NACL_SUBARCH_NAME(ApplyValidatorVerbosely,
                               NACL_TARGET_ARCH,
                               NACL_TARGET_SUBARCH)
        (load_address, code, code_size, cpu_features);
  }

  result = FixUpSectionCheckStatus(status);
  free(cpu_features);
  return result;
}

static void CheckBounds(unsigned char *data, size_t data_size,
                        void *ptr, size_t inside_size) {
  CHECK(data <= (unsigned char *) ptr);
  CHECK((unsigned char *) ptr + inside_size <= data + data_size);
}

static Bool FixUpELF32(unsigned char *data, size_t data_size) {
  Elf32_Ehdr *header;
  int index;
  Bool fixed = TRUE;  /* until proven otherwise. */

  header = (Elf32_Ehdr *) data;
  CheckBounds(data, data_size, header, sizeof(*header));
  CHECK(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

  for (index = 0; index < header->e_shnum; index++) {
    Elf32_Shdr *section = (Elf32_Shdr *) (data + header->e_shoff +
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

#if NACL_TARGET_SUBARCH == 64
static Bool FixUpELF64(unsigned char *data, size_t data_size) {
  Elf64_Ehdr *header;
  int index;
  Bool fixed = TRUE;  /* until proven otherwise. */

  header = (Elf64_Ehdr *) data;
  CheckBounds(data, data_size, header, sizeof(*header));
  CHECK(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

  for (index = 0; index < header->e_shnum; index++) {
    Elf64_Shdr *section = (Elf64_Shdr *) (data + header->e_shoff +
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
#endif

static Bool FixUpELF(unsigned char *data, size_t data_size) {
#if NACL_TARGET_SUBARCH == 64
  if (data_size > EI_CLASS && data[EI_CLASS] == ELFCLASS64)
    return FixUpELF64(data, data_size);
#endif
  return FixUpELF32(data, data_size);
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
