/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl helper functions to deal with elf images
 */

#include "native_client/src/include/portability.h"

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#define NACL_LOG_MODULE_NAME  "elf_util"

#include "native_client/src/include/elf_constants.h"
#include "native_client/src/include/elf.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/elf_util.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/validator/validation_metadata.h"

/* private */
struct NaClElfImage {
  Elf_Ehdr  ehdr;
  Elf_Phdr  phdrs[NACL_MAX_PROGRAM_HEADERS];
  int       loadable[NACL_MAX_PROGRAM_HEADERS];
};


enum NaClPhdrCheckAction {
  PCA_NONE,
  PCA_TEXT_CHECK,
  PCA_RODATA,
  PCA_DATA,
  PCA_IGNORE  /* ignore this segment. */
};


struct NaClPhdrChecks {
  Elf_Word                  p_type;
  Elf_Word                  p_flags;  /* rwx */
  enum NaClPhdrCheckAction  action;
  int                       required;  /* only for text for now */
  Elf_Addr                  p_vaddr;  /* if non-zero, vaddr must be this */
};

/*
 * Other than empty segments, these are the only ones that are allowed.
 */
static const struct NaClPhdrChecks nacl_phdr_check_data[] = {
  /* phdr */
  { PT_PHDR, PF_R, PCA_IGNORE, 0, 0, },
  /* text */
  { PT_LOAD, PF_R|PF_X, PCA_TEXT_CHECK, 1, NACL_TRAMPOLINE_END, },
  /* rodata */
  { PT_LOAD, PF_R, PCA_RODATA, 0, 0, },
  /* data/bss */
  { PT_LOAD, PF_R|PF_W, PCA_DATA, 0, 0, },
  /* tls */
  { PT_TLS, PF_R, PCA_IGNORE, 0, 0},
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  /* arm exception handling unwind info (for c++)*/
  /* TODO(robertm): for some reason this does NOT end up in ro maybe because
   *             it is relocatable. Try hacking the linker script to move it.
   */
  { PT_ARM_EXIDX, PF_R, PCA_IGNORE, 0, 0, },
#endif
  /*
   * allow optional GNU stack permission marker, but require that the
   * stack is non-executable.
   */
  { PT_GNU_STACK, PF_R|PF_W, PCA_NONE, 0, 0, },
  /* ignored segments */
  { PT_DYNAMIC, PF_R, PCA_IGNORE, 0, 0},
  { PT_INTERP, PF_R, PCA_IGNORE, 0, 0},
  { PT_NOTE, PF_R, PCA_IGNORE, 0, 0},
  { PT_GNU_EH_FRAME, PF_R, PCA_IGNORE, 0, 0},
  { PT_GNU_RELRO, PF_R, PCA_IGNORE, 0, 0},
  { PT_NULL, PF_R, PCA_IGNORE, 0, 0},
};


#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
# if NACL_BUILD_SUBARCH == 32
#  define EM_EXPECTED_BY_NACL EM_386
# elif NACL_BUILD_SUBARCH == 64
#  define EM_EXPECTED_BY_NACL EM_X86_64
# else
#  error "No NACL_BUILD_SUBARCH for x86 -- are we on x86-128?"
# endif
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
# define EM_EXPECTED_BY_NACL EM_ARM
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
# define EM_EXPECTED_BY_NACL EM_MIPS
#else
# error "Unknown platform!"
#endif


static void NaClDumpElfHeader(int loglevel, Elf_Ehdr *elf_hdr) {

#define DUMP(m,f)    do { NaClLog(loglevel,                     \
                                  #m " = %" f "\n",             \
                                  elf_hdr->m); } while (0)

  NaClLog(loglevel, "=================================================\n");
  NaClLog(loglevel, "Elf header\n");
  NaClLog(loglevel, "==================================================\n");

  DUMP(e_ident+1, ".3s");
  DUMP(e_type, "#x");
  DUMP(e_machine, "#x");
  DUMP(e_version, "#x");
  DUMP(e_entry, "#"NACL_PRIxElf_Addr);
  DUMP(e_phoff, "#"NACL_PRIxElf_Off);
  DUMP(e_shoff, "#"NACL_PRIxElf_Off);
  DUMP(e_flags, "#"NACL_PRIxElf_Word);
  DUMP(e_ehsize, "#"NACL_PRIxElf_Half);
  DUMP(e_phentsize, "#"NACL_PRIxElf_Half);
  DUMP(e_phnum, "#"NACL_PRIxElf_Half);
  DUMP(e_shentsize, "#"NACL_PRIxElf_Half);
  DUMP(e_shnum, "#"NACL_PRIxElf_Half);
  DUMP(e_shstrndx, "#"NACL_PRIxElf_Half);
#undef DUMP
 NaClLog(loglevel, "sizeof(Elf32_Ehdr) = 0x%x\n", (int) sizeof *elf_hdr);
}


static void NaClDumpElfProgramHeader(int     loglevel,
                                    Elf_Phdr *phdr) {
#define DUMP(mem, f) do {                                \
    NaClLog(loglevel, "%s: %" f "\n", #mem, phdr->mem);  \
  } while (0)

  DUMP(p_type, NACL_PRIxElf_Word);
  DUMP(p_offset, NACL_PRIxElf_Off);
  DUMP(p_vaddr, NACL_PRIxElf_Addr);
  DUMP(p_paddr, NACL_PRIxElf_Addr);
  DUMP(p_filesz, NACL_PRIxElf_Xword);
  DUMP(p_memsz, NACL_PRIxElf_Xword);
  DUMP(p_flags, NACL_PRIxElf_Word);
  NaClLog(2, " (%s %s %s)\n",
          (phdr->p_flags & PF_R) ? "PF_R" : "",
          (phdr->p_flags & PF_W) ? "PF_W" : "",
          (phdr->p_flags & PF_X) ? "PF_X" : "");
  DUMP(p_align, NACL_PRIxElf_Xword);
#undef  DUMP
  NaClLog(loglevel, "\n");
}


NaClErrorCode NaClElfImageValidateElfHeader(struct NaClElfImage *image) {
  const Elf_Ehdr *hdr = &image->ehdr;

  if (memcmp(hdr->e_ident, ELFMAG, SELFMAG)) {
    NaClLog(LOG_ERROR, "bad elf magic\n");
    return LOAD_BAD_ELF_MAGIC;
  }

  if (ELFCLASS32 != hdr->e_ident[EI_CLASS]) {
    NaClLog(LOG_ERROR, "bad elf class\n");
    return LOAD_NOT_32_BIT;
  }

  if (ET_EXEC != hdr->e_type) {
    NaClLog(LOG_ERROR, "non executable\n");
    return LOAD_NOT_EXEC;
  }

  if (EM_EXPECTED_BY_NACL != hdr->e_machine) {
    NaClLog(LOG_ERROR, "bad machine: %"NACL_PRIxElf_Half"\n", hdr->e_machine);
    return LOAD_BAD_MACHINE;
  }

  if (EV_CURRENT != hdr->e_version) {
    NaClLog(LOG_ERROR, "bad elf version: %"NACL_PRIxElf_Word"\n",
            hdr->e_version);
    return LOAD_BAD_ELF_VERS;
  }

  return LOAD_OK;
}

/* TODO(robertm): decouple validation from computation of
                   static_text_end and max_vaddr */
NaClErrorCode NaClElfImageValidateProgramHeaders(
  struct NaClElfImage *image,
  uint8_t             addr_bits,
  struct NaClElfImageInfo *info) {
    /*
     * Scan phdrs and do sanity checks in-line.  Verify that the load
     * address is NACL_TRAMPOLINE_END, that we have a single text
     * segment.  Data and TLS segments are not required, though it is
     * hard to avoid with standard tools, but in any case there should
     * be at most one each.  Ensure that no segment's vaddr is outside
     * of the address space.  Ensure that PT_GNU_STACK is present, and
     * that x is off.
     */
  const Elf_Ehdr      *hdr = &image->ehdr;
  int                 seen_seg[NACL_ARRAY_SIZE(nacl_phdr_check_data)];

  int                 segnum;
  const Elf_Phdr      *php;
  size_t              j;

  memset(info, 0, sizeof(*info));

  info->max_vaddr = NACL_TRAMPOLINE_END;

  /*
   * nacl_phdr_check_data is small, so O(|check_data| * nap->elf_hdr.e_phum)
   * is okay.
   */
  memset(seen_seg, 0, sizeof seen_seg);
  for (segnum = 0; segnum < hdr->e_phnum; ++segnum) {
    php = &image->phdrs[segnum];
    NaClLog(3, "Looking at segment %d, type 0x%x, p_flags 0x%x\n",
            segnum, php->p_type, php->p_flags);
    if (0 == php->p_memsz) {
      /*
       * We will not load this segment.
       */
      NaClLog(3, "Ignoring empty segment\n");
      continue;
    }

    for (j = 0; j < NACL_ARRAY_SIZE(nacl_phdr_check_data); ++j) {
      if (php->p_type == nacl_phdr_check_data[j].p_type
          && php->p_flags == nacl_phdr_check_data[j].p_flags)
        break;
    }
    if (j == NACL_ARRAY_SIZE(nacl_phdr_check_data)) {
      /* segment not in nacl_phdr_check_data */
      NaClLog(2,
              "Segment %d is of unexpected type 0x%x, flag 0x%x\n",
              segnum,
              php->p_type,
              php->p_flags);
      return LOAD_BAD_SEGMENT;
    }

    NaClLog(2, "Matched nacl_phdr_check_data[%"NACL_PRIdS"]\n", j);
    if (seen_seg[j]) {
      NaClLog(2, "Segment %d is a type that has been seen\n", segnum);
      return LOAD_DUP_SEGMENT;
    }
    seen_seg[j] = 1;

    if (PCA_IGNORE == nacl_phdr_check_data[j].action) {
      NaClLog(3, "Ignoring\n");
      continue;
    }

    /*
     * We will load this segment later.  Do the sanity checks.
     */
    if (0 != nacl_phdr_check_data[j].p_vaddr
        && (nacl_phdr_check_data[j].p_vaddr != php->p_vaddr)) {
      NaClLog(2,
              ("Segment %d: bad virtual address: 0x%08"
               NACL_PRIxElf_Addr","
               " expected 0x%08"NACL_PRIxElf_Addr"\n"),
              segnum,
              php->p_vaddr,
              nacl_phdr_check_data[j].p_vaddr);
      return LOAD_SEGMENT_BAD_LOC;
    }
    if (php->p_vaddr < NACL_TRAMPOLINE_END) {
      NaClLog(2,
              ("Segment %d: virtual address (0x%08"NACL_PRIxElf_Addr
               ") too low\n"),
              segnum,
              php->p_vaddr);
      return LOAD_SEGMENT_OUTSIDE_ADDRSPACE;
    }
    if (php->p_vaddr >= ((uint64_t) 1U << addr_bits) ||
        ((uint64_t) 1U << addr_bits) - php->p_vaddr < php->p_memsz) {
      if (php->p_vaddr + php->p_memsz < php->p_vaddr) {
        NaClLog(2,
                "Segment %d: p_memsz caused integer overflow\n",
                segnum);
      } else {
        NaClLog(2,
                "Segment %d: too large, ends at 0x%08"NACL_PRIxElf_Addr"\n",
                segnum,
                php->p_vaddr + php->p_memsz);
      }
      return LOAD_SEGMENT_OUTSIDE_ADDRSPACE;
    }
    if (php->p_filesz > php->p_memsz) {
      NaClLog(2,
              ("Segment %d: file size 0x%08"NACL_PRIxElf_Xword" larger"
               " than memory size 0x%08"NACL_PRIxElf_Xword"\n"),
              segnum,
              php->p_filesz,
              php->p_memsz);
      return LOAD_SEGMENT_BAD_PARAM;
    }

    image->loadable[segnum] = 1;
    /* record our decision that we will load this segment */

    /*
     * NACL_TRAMPOLINE_END <= p_vaddr
     *                     <= p_vaddr + p_memsz
     *                     < ((uintptr_t) 1U << nap->addr_bits)
     */
    if (info->max_vaddr < php->p_vaddr + php->p_memsz) {
      info->max_vaddr = php->p_vaddr + php->p_memsz;
    }

    switch (nacl_phdr_check_data[j].action) {
      case PCA_NONE:
        break;
      case PCA_TEXT_CHECK:
        if (0 == php->p_memsz) {
          return LOAD_BAD_ELF_TEXT;
        }
        info->static_text_end = NACL_TRAMPOLINE_END + php->p_filesz;
        break;
      case PCA_RODATA:
        info->rodata_start = php->p_vaddr;
        info->rodata_end = php->p_vaddr + php->p_memsz;
        break;
      case PCA_DATA:
        info->data_start = php->p_vaddr;
        info->data_end = php->p_vaddr + php->p_memsz;
        break;
      case PCA_IGNORE:
        break;
    }
  }
  for (j = 0; j < NACL_ARRAY_SIZE(nacl_phdr_check_data); ++j) {
    if (nacl_phdr_check_data[j].required && !seen_seg[j]) {
      return LOAD_REQUIRED_SEG_MISSING;
    }
  }

  return LOAD_OK;
}


struct NaClElfImage *NaClElfImageNew(struct Gio     *gp,
                                     NaClErrorCode  *err_code) {
  struct NaClElfImage *result;
  struct NaClElfImage image;
  union {
    Elf32_Ehdr ehdr32;
#if NACL_TARGET_SUBARCH == 64
    Elf64_Ehdr ehdr64;
#endif
  } ehdr;
  int                 cur_ph;

  memset(image.loadable, 0, sizeof image.loadable);
  if (-1 == (*gp->vtbl->Seek)(gp, 0, 0)) {
    NaClLog(2, "could not seek to beginning of Gio object containing nexe\n");
    *err_code = LOAD_READ_ERROR;
    return 0;
  }

  /*
   * We read the larger size of an ELFCLASS64 header even if it turns out
   * we're reading an ELFCLASS32 file.  No usable ELFCLASS32 binary could
   * be so small that it's not larger than Elf64_Ehdr anyway.
   */
  if ((*gp->vtbl->Read)(gp,
                        &ehdr,
                        sizeof ehdr)
      != sizeof ehdr) {
    *err_code = LOAD_READ_ERROR;
    NaClLog(2, "could not load elf headers\n");
    return 0;
  }

#if NACL_TARGET_SUBARCH == 64
  if (ELFCLASS64 == ehdr.ehdr64.e_ident[EI_CLASS]) {
    /*
     * Convert ELFCLASS64 format to ELFCLASS32 format.
     * The initial four fields are the same in both classes.
     */
    memcpy(image.ehdr.e_ident, ehdr.ehdr64.e_ident, EI_NIDENT);
    image.ehdr.e_ident[EI_CLASS] = ELFCLASS32;
    image.ehdr.e_type = ehdr.ehdr64.e_type;
    image.ehdr.e_machine = ehdr.ehdr64.e_machine;
    image.ehdr.e_version = ehdr.ehdr64.e_version;
    if (ehdr.ehdr64.e_entry > 0xffffffffU ||
        ehdr.ehdr64.e_phoff > 0xffffffffU ||
        ehdr.ehdr64.e_shoff > 0xffffffffU) {
      *err_code = LOAD_EHDR_OVERFLOW;
      NaClLog(2, "ELFCLASS64 file header fields overflow 32 bits\n");
      return 0;
    }
    image.ehdr.e_entry = (Elf32_Addr) ehdr.ehdr64.e_entry;
    image.ehdr.e_phoff = (Elf32_Off) ehdr.ehdr64.e_phoff;
    image.ehdr.e_shoff = (Elf32_Off) ehdr.ehdr64.e_shoff;
    image.ehdr.e_flags = ehdr.ehdr64.e_flags;
    if (ehdr.ehdr64.e_ehsize != sizeof(ehdr.ehdr64)) {
      *err_code = LOAD_BAD_EHSIZE;
      NaClLog(2, "ELFCLASS64 file e_ehsize != %d\n", (int) sizeof(ehdr.ehdr64));
      return 0;
    }
    image.ehdr.e_ehsize = sizeof(image.ehdr);
    image.ehdr.e_phentsize = sizeof(image.phdrs[0]);
    image.ehdr.e_phnum = ehdr.ehdr64.e_phnum;
    image.ehdr.e_shentsize = ehdr.ehdr64.e_shentsize;
    image.ehdr.e_shnum = ehdr.ehdr64.e_shnum;
    image.ehdr.e_shstrndx = ehdr.ehdr64.e_shstrndx;
  } else
#endif
  {
    image.ehdr = ehdr.ehdr32;
  }

  NaClDumpElfHeader(2, &image.ehdr);

  *err_code = NaClElfImageValidateElfHeader(&image);
  if (LOAD_OK != *err_code) {
    return 0;
  }

  /* read program headers */
  if (image.ehdr.e_phnum > NACL_MAX_PROGRAM_HEADERS) {
    *err_code = LOAD_TOO_MANY_PROG_HDRS;
    NaClLog(2, "too many prog headers\n");
    return 0;
  }

  if ((*gp->vtbl->Seek)(gp,
                        (off_t) image.ehdr.e_phoff,
                        SEEK_SET) == (off_t) -1) {
    *err_code = LOAD_READ_ERROR;
    NaClLog(2, "cannot seek tp prog headers\n");
    return 0;
  }

#if NACL_TARGET_SUBARCH == 64
  if (ELFCLASS64 == ehdr.ehdr64.e_ident[EI_CLASS]) {
    /*
     * We'll load the 64-bit phdrs and convert them to 32-bit format.
     */
    Elf64_Phdr phdr64[NACL_MAX_PROGRAM_HEADERS];

    if (ehdr.ehdr64.e_phentsize != sizeof(Elf64_Phdr)) {
      *err_code = LOAD_BAD_PHENTSIZE;
      NaClLog(2, "bad prog headers size\n");
      NaClLog(2, " ehdr64.e_phentsize = 0x%"NACL_PRIxElf_Half"\n",
              ehdr.ehdr64.e_phentsize);
      NaClLog(2, "  sizeof(Elf64_Phdr) = 0x%"NACL_PRIxS"\n",
              sizeof(Elf64_Phdr));
      return 0;
    }

    /*
     * We know the multiplication won't overflow since we rejected
     * e_phnum values larger than the small constant NACL_MAX_PROGRAM_HEADERS.
     */
    if ((size_t) (*gp->vtbl->Read)(gp,
                                   &phdr64[0],
                                   image.ehdr.e_phnum * sizeof phdr64[0])
        != (image.ehdr.e_phnum * sizeof phdr64[0])) {
      *err_code = LOAD_READ_ERROR;
      NaClLog(2, "cannot load tp prog headers\n");
      return 0;
    }

    for (cur_ph = 0; cur_ph < image.ehdr.e_phnum; ++cur_ph) {
      if (phdr64[cur_ph].p_offset > 0xffffffffU ||
          phdr64[cur_ph].p_vaddr > 0xffffffffU ||
          phdr64[cur_ph].p_paddr > 0xffffffffU ||
          phdr64[cur_ph].p_filesz > 0xffffffffU ||
          phdr64[cur_ph].p_memsz > 0xffffffffU ||
          phdr64[cur_ph].p_align > 0xffffffffU) {
        *err_code = LOAD_PHDR_OVERFLOW;
        NaClLog(2, "ELFCLASS64 program header fields overflow 32 bits\n");
        return 0;
      }
      image.phdrs[cur_ph].p_type = phdr64[cur_ph].p_type;
      image.phdrs[cur_ph].p_offset = (Elf32_Off) phdr64[cur_ph].p_offset;
      image.phdrs[cur_ph].p_vaddr = (Elf32_Addr) phdr64[cur_ph].p_vaddr;
      image.phdrs[cur_ph].p_paddr = (Elf32_Addr) phdr64[cur_ph].p_paddr;
      image.phdrs[cur_ph].p_filesz = (Elf32_Word) phdr64[cur_ph].p_filesz;
      image.phdrs[cur_ph].p_memsz = (Elf32_Word) phdr64[cur_ph].p_memsz;
      image.phdrs[cur_ph].p_flags = phdr64[cur_ph].p_flags;
      image.phdrs[cur_ph].p_align = (Elf32_Word) phdr64[cur_ph].p_align;
    }
  } else
#endif
  {
    if (image.ehdr.e_phentsize != sizeof image.phdrs[0]) {
      *err_code = LOAD_BAD_PHENTSIZE;
      NaClLog(2, "bad prog headers size\n");
      NaClLog(2, " image.ehdr.e_phentsize = 0x%"NACL_PRIxElf_Half"\n",
              image.ehdr.e_phentsize);
      NaClLog(2, "  sizeof image.phdrs[0] = 0x%"NACL_PRIxS"\n",
              sizeof image.phdrs[0]);
      return 0;
    }

    if ((size_t) (*gp->vtbl->Read)(gp,
                                   &image.phdrs[0],
                                   image.ehdr.e_phnum * sizeof image.phdrs[0])
        != (image.ehdr.e_phnum * sizeof image.phdrs[0])) {
      *err_code = LOAD_READ_ERROR;
      NaClLog(2, "cannot load tp prog headers\n");
      return 0;
    }
  }

  NaClLog(2, "=================================================\n");
  NaClLog(2, "Elf Program headers\n");
  NaClLog(2, "==================================================\n");
  for (cur_ph = 0; cur_ph <  image.ehdr.e_phnum; ++cur_ph) {
    NaClDumpElfProgramHeader(2, &image.phdrs[cur_ph]);
  }

  /* we delay allocating till the end to avoid cleanup code */
  result = malloc(sizeof image);
  if (result == 0) {
    *err_code = LOAD_NO_MEMORY;
    NaClLog(LOG_FATAL, "no enough memory for image meta data\n");
    return 0;
  }
  memcpy(result, &image, sizeof image);
  *err_code = LOAD_OK;
  return result;
}


NaClErrorCode NaClElfImageLoad(struct NaClElfImage *image,
                               struct Gio          *gp,
                               uint8_t             addr_bits,
                               uintptr_t           mem_start) {
  int               segnum;
  uintptr_t         paddr;
  uintptr_t         end_vaddr;

  for (segnum = 0; segnum < image->ehdr.e_phnum; ++segnum) {
    const Elf_Phdr *php = &image->phdrs[segnum];
    Elf_Off offset = (Elf_Off) NaClTruncAllocPage(php->p_offset);
    Elf_Off filesz = php->p_offset + php->p_filesz - offset;

    /* did we decide that we will load this segment earlier? */
    if (!image->loadable[segnum]) {
      continue;
    }

    NaClLog(2, "loading segment %d\n", segnum);

    if (0 == php->p_filesz) {
      NaClLog(4, "zero-sized segment.  ignoring...\n");
      continue;
    }

    end_vaddr = php->p_vaddr + php->p_filesz;
    /* integer overflow? */
    if (end_vaddr < php->p_vaddr) {
      NaClLog(LOG_FATAL, "parameter error should have been detected already\n");
    }
    /*
     * is the end virtual address within the NaCl application's
     * address space?  if it is, it implies that the start virtual
     * address is also.
     */
    if (end_vaddr >= ((uintptr_t) 1U << addr_bits)) {
      NaClLog(LOG_FATAL, "parameter error should have been detected already\n");
    }

    paddr = mem_start + NaClTruncAllocPage(php->p_vaddr);

    NaClLog(4,
            "Seek to position %"NACL_PRIdElf_Off" (0x%"NACL_PRIxElf_Off").\n",
            offset,
            offset);

    /*
     * NB: php->p_offset may not be a valid off_t on 64-bit systems, but
     * in that case Seek() will error out.
     */
    if ((*gp->vtbl->Seek)(gp, (off_t) offset, SEEK_SET) == (off_t) -1) {
      NaClLog(LOG_ERROR, "seek failure segment %d", segnum);
      return LOAD_SEGMENT_BAD_PARAM;
    }
    NaClLog(4,
            "Reading %"NACL_PRIdElf_Xword" (0x%"NACL_PRIxElf_Xword") bytes to"
            " address 0x%"NACL_PRIxPTR"\n",
            filesz,
            filesz,
            paddr);

    /*
     * Tell valgrind that this memory is accessible and undefined. For more
     * details see
     * http://code.google.com/p/nativeclient/wiki/ValgrindMemcheck#Implementation_details
     */
    NACL_MAKE_MEM_UNDEFINED((void *) paddr, filesz);

    if ((Elf_Word) (*gp->vtbl->Read)(gp, (void *) paddr, filesz) != filesz) {
      NaClLog(LOG_ERROR, "load failure segment %d", segnum);
      return LOAD_SEGMENT_BAD_PARAM;
    }
    /* region from p_filesz to p_memsz should already be zero filled */

    /* Tell Valgrind that we've mapped a segment of nacl_file. */
    NaClFileMappingForValgrind(paddr, filesz, offset);
  }

  return LOAD_OK;
}


NaClErrorCode NaClElfImageLoadDynamically(
    struct NaClElfImage *image,
    struct NaClApp *nap,
    struct Gio *gfile,
    struct NaClValidationMetadata *metadata) {
  int segnum;
  for (segnum = 0; segnum < image->ehdr.e_phnum; ++segnum) {
    const Elf_Phdr *php = &image->phdrs[segnum];
    Elf_Addr vaddr = php->p_vaddr & ~(NACL_MAP_PAGESIZE - 1);
    Elf_Off offset = php->p_offset & ~(NACL_MAP_PAGESIZE - 1);
    Elf_Off filesz = php->p_offset + php->p_filesz - offset;
    Elf_Off memsz = php->p_offset + php->p_memsz - offset;
    int32_t result;

    /*
     * We check for PT_LOAD directly rather than using the "loadable"
     * array because we are not using NaClElfImageValidateProgramHeaders()
     * to fill out the "loadable" array for this ELF object.  This ELF
     * object does not have to fit such strict constraints (such as
     * having code at 0x20000), and safety checks are applied by
     * NaClTextDyncodeCreate() and NaClSysMmapIntern().
     */
    if (PT_LOAD != php->p_type) {
      continue;
    }

    /*
     * Ideally, Gio would have a Pread() method which we would use
     * instead of Seek().  In practice, though, there is no
     * Seek()/Read() race condition here because both
     * GioMemoryFileSnapshot and NaClGioShm use a seek position that
     * is local and not shared between processes.
     */
    if ((*gfile->vtbl->Seek)(gfile, (off_t) offset, SEEK_SET) == (off_t) -1) {
      NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: seek failed\n");
      return LOAD_READ_ERROR;
    }

    if (0 != (php->p_flags & PF_X)) {
      /* Load code segment. */
      /*
       * We make a copy of the code.  This is not ideal given that
       * GioMemoryFileSnapshot and NaClGioShm already have a copy of
       * the file in memory or mmapped.
       * TODO(mseaborn): Reduce the amount of copying here.
       */
      char *code_copy = malloc(filesz);
      if (NULL == code_copy) {
        NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: malloc failed\n");
        return LOAD_NO_MEMORY;
      }
      if ((Elf_Word) (*gfile->vtbl->Read)(gfile, code_copy, filesz) != filesz) {
        free(code_copy);
        NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: "
                "failed to read code segment\n");
        return LOAD_READ_ERROR;
      }
      if (NULL != metadata) {
        metadata->code_offset = offset;
      }
      result = NaClTextDyncodeCreate(nap, (uint32_t) vaddr,
                                     code_copy, (uint32_t) filesz, metadata);
      free(code_copy);
      if (0 != result) {
        NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: "
                "failed to load code segment\n");
        return LOAD_UNLOADABLE;
      }
    } else {
      /* Load data segment. */
      void *paddr = (void *) NaClUserToSys(nap, vaddr);
      size_t mapping_size = NaClRoundAllocPage(memsz);
      /*
       * Note that we do not used NACL_ABI_MAP_FIXED because we do not
       * want to silently overwrite any existing mappings, such as the
       * user app's data segment or the stack.  We detect overmapping
       * when mmap chooses not to use the preferred address we supply.
       * (Ideally mmap would provide a MAP_EXCL option for this
       * instead.)
       */
      result = NaClSysMmapIntern(
          nap, (void *) (uintptr_t) vaddr, mapping_size,
          NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
          NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE,
          -1, 0);
      if ((int32_t) vaddr != result) {
        NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: "
                "failed to map data segment\n");
        return LOAD_UNLOADABLE;
      }
      if ((Elf_Word) (*gfile->vtbl->Read)(gfile, paddr, filesz) != filesz) {
        NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: "
                "failed to read data segment\n");
        return LOAD_READ_ERROR;
      }
      /*
       * Note that we do not need to zero the BSS (the region from
       * p_filesz to p_memsz) because it should already be zero
       * filled.  This would not be the case if we were mapping the
       * data segment from the file.
       */

      if (0 == (php->p_flags & PF_W)) {
        /* Handle read-only data segment. */
        int rc = NaClMprotect(paddr, mapping_size, NACL_ABI_PROT_READ);
        if (0 != rc) {
          NaClLog(LOG_ERROR, "NaClElfImageLoadDynamically: "
                  "failed to mprotect read-only data segment\n");
          return LOAD_MPROTECT_FAIL;
        }

        NaClVmmapAddWithOverwrite(&nap->mem_map,
                                  vaddr >> NACL_PAGESHIFT,
                                  mapping_size >> NACL_PAGESHIFT,
                                  PROT_READ,
                                  PROT_READ,
                                  NACL_VMMAP_ENTRY_ANONYMOUS);
      }
    }
  }
  return LOAD_OK;
}


void NaClElfImageDelete(struct NaClElfImage *image) {
  free(image);
}


uintptr_t NaClElfImageGetEntryPoint(struct NaClElfImage *image) {
  return image->ehdr.e_entry;
}
