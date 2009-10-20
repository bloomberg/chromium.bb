/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

#include "native_client/src/include/portability.h"

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/elf_constants.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"
#include "native_client/src/trusted/service_runtime/nacl_sync_queue.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/tramp.h"

#define PTR_ALIGN_MASK  ((sizeof(void *))-1)

/*
 * Other than empty segments, these are the only ones that are allowed.
 */
struct NaClPhdrChecks nacl_phdr_check_data[] = {
  /* phdr */
  { PT_PHDR, PF_R, PCA_IGNORE, 0, 0, },
  /* text */
  { PT_LOAD, PF_R|PF_X, PCA_TEXT_CHECK, 1, NACL_TRAMPOLINE_END, },
  /* rodata */
  { PT_LOAD, PF_R, PCA_NONE, 0, 0, },
  /* data/bss */
  { PT_LOAD, PF_R|PF_W, PCA_NONE, 0, 0, },
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
};


NaClErrorCode NaClProcessPhdrs(struct NaClApp *nap) {
    /* Scan phdrs and do sanity checks in-line.  Verify that the load
     * address is NACL_TRAMPOLINE_END, that we have a single text
     * segment.  Data and TLS segments are not required, though it is
     * hard to avoid with standard tools, but in any case there should
     * be at most one each.  Ensure that no segment's vaddr is outside
     * of the address space.  Ensure that PT_GNU_STACK is present, and
     * that x is off.
     */
  int         seen_seg[NACL_ARRAY_SIZE(nacl_phdr_check_data)];

  int         segnum;
  Elf32_Phdr  *php;
  size_t      j;
  uintptr_t   max_vaddr;

  memset(seen_seg, 0, sizeof seen_seg);
  max_vaddr = NACL_TRAMPOLINE_END;
  /*
   * nacl_phdr_check_data is small, so O(|check_data| * nap->elf_hdr.e_phum)
   * is okay.
   */
  for (segnum = 0; segnum < nap->elf_hdr.e_phnum; ++segnum) {
    php = &nap->phdrs[segnum];
    NaClLog(3, "Looking at segment %d, type 0x%x, p_flags 0x%x\n",
            segnum, php->p_type, php->p_flags);
    php->p_flags &= ~PF_MASKOS;
    for (j = 0;
         j < NACL_ARRAY_SIZE(nacl_phdr_check_data);
         ++j) {
      if (php->p_type == nacl_phdr_check_data[j].p_type
          && php->p_flags == nacl_phdr_check_data[j].p_flags) {
        NaClLog(2, "Matched nacl_phdr_check_data[%"PRIdS"]\n", j);
        if (seen_seg[j]) {
          NaClLog(2, "Segment %d is a type that has been seen\n", segnum);
          return LOAD_DUP_SEGMENT;
        }
        ++seen_seg[j];

        if (PCA_IGNORE == nacl_phdr_check_data[j].action) {
          NaClLog(3, "Ignoring\n");
          goto next_seg;
        }

        if (0 != php->p_memsz) {
          /*
           * We will load this segment later.  Do the sanity checks.
           */
          if (0 != nacl_phdr_check_data[j].p_vaddr
              && (nacl_phdr_check_data[j].p_vaddr != php->p_vaddr)) {
            NaClLog(2,
                    ("Segment %d: bad virtual address: 0x%08x,"
                     " expected 0x%08x\n"),
                    segnum,
                    php->p_vaddr,
                    nacl_phdr_check_data[j].p_vaddr);
            return LOAD_SEGMENT_BAD_LOC;
          }
          if (php->p_vaddr < NACL_TRAMPOLINE_END) {
            NaClLog(2, "Segment %d: virtual address (0x%08x) too low\n",
                    segnum,
                    php->p_vaddr);
            return LOAD_SEGMENT_OUTSIDE_ADDRSPACE;
          }
          /*
           * integer overflow?  Elf32_Addr and Elf32_Word are uint32_t,
           * so the addition/comparison is well defined.
           */
          if (php->p_vaddr + php->p_memsz < php->p_vaddr) {
            NaClLog(2,
                    "Segment %d: p_memsz caused integer overflow\n",
                    segnum);
            return LOAD_SEGMENT_OUTSIDE_ADDRSPACE;
          }
          if (php->p_vaddr + php->p_memsz >= (1U << nap->addr_bits)) {
            NaClLog(2,
                    "Segment %d: too large, ends at 0x%08x\n",
                    segnum,
                    php->p_vaddr + php->p_memsz);
            return LOAD_SEGMENT_OUTSIDE_ADDRSPACE;
          }
          if (php->p_filesz > php->p_memsz) {
            NaClLog(2,
                    ("Segment %d: file size 0x%08x larger"
                     " than memory size 0x%08x\n"),
                    segnum,
                    php->p_filesz,
                    php->p_memsz);
            return LOAD_SEGMENT_BAD_PARAM;
          }

          php->p_flags |= PF_OS_WILL_LOAD;
          /* record our decision that we will load this segment */

          /*
           * NACL_TRAMPOLINE_END <= p_vaddr
           *                     <= p_vaddr + p_memsz
           *                     < (1U << nap->addr_bits)
           */
          if (max_vaddr < php->p_vaddr + php->p_memsz) {
            max_vaddr = php->p_vaddr + php->p_memsz;
          }
        }

        switch (nacl_phdr_check_data[j].action) {
          case PCA_NONE:
            break;
          case PCA_TEXT_CHECK:
            if (0 == php->p_memsz) {
              return LOAD_BAD_ELF_TEXT;
            }
            nap->text_region_bytes = php->p_filesz;
            break;
          case PCA_IGNORE:
            break;
        }
        goto next_seg;
      }
    }
    /* segment not in nacl_phdr_check_data */
    if (0 == php->p_memsz) {
      NaClLog(3, "Segment %d zero size: ignored\n", segnum);
      continue;
    }
    NaClLog(2,
            "Segment %d is of unexpected type 0x%x, flag 0x%x\n",
            segnum,
            php->p_type,
            php->p_flags);
    return LOAD_BAD_SEGMENT;
 next_seg:
    ;
  }
  for (j = 0;
       j < NACL_ARRAY_SIZE(nacl_phdr_check_data);
       ++j) {
    if (nacl_phdr_check_data[j].required && !seen_seg[j]) {
      return LOAD_REQUIRED_SEG_MISSING;
    }
  }
  nap->data_end = nap->break_addr = max_vaddr;
  /*
   * Memory allocation will use NaClRoundPage(nap->break_addr), but
   * the system notion of break is always an exact address.  Even
   * though we must allocate and make accessible multiples of pages,
   * the linux-style brk system call (which returns current break on
   * failure) permits an arbitrarily aligned address as argument.
   */

  return LOAD_OK;
}


static void NaClDumpElfHeader(Elf32_Ehdr *elf_hdr) {
#define DUMP(m,f)    do { NaClLog(2,                            \
                                  #m " = %" f "\n",             \
                                  elf_hdr->m); } while (0)
  DUMP(e_ident+1, ".3s");
  DUMP(e_type, "#x");
  DUMP(e_machine, "#x");
  DUMP(e_version, "#x");
  DUMP(e_entry, "#x");
  DUMP(e_phoff, "#x");
  DUMP(e_shoff, "#x");
  DUMP(e_flags, "#x");
  DUMP(e_ehsize, "#x");
  DUMP(e_phentsize, "#x");
  DUMP(e_phnum, "#x");
  DUMP(e_shentsize, "#x");
  DUMP(e_shnum, "#x");
  DUMP(e_shstrndx, "#x");
#undef DUMP
 NaClLog(2, "sizeof(Elf32_Ehdr) = %x\n", (int) sizeof *elf_hdr);
}


static NaClErrorCode NaClValidateElfHeader(Elf32_Ehdr *hdr,
                                           enum NaClAbiMismatchOption
                                           abi_mismatch_option) {
 if (memcmp(hdr->e_ident, ELFMAG, SELFMAG)) {
    return LOAD_BAD_ELF_MAGIC;
  }
  if (ELFCLASS32 != hdr->e_ident[EI_CLASS]) {
    return LOAD_NOT_32_BIT;
  }

#if !defined(DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX)
  if (ELFOSABI_NACL != hdr->e_ident[EI_OSABI]) {
    NaClLog(LOG_ERROR, "Expected OSABI %d, got %d\n",
            ELFOSABI_NACL,
            hdr->e_ident[EI_OSABI]);
    if (abi_mismatch_option == NACL_ABI_MISMATCH_OPTION_ABORT) {
      return LOAD_BAD_ABI;
    }
  }

  if (EF_NACL_ABIVERSION != hdr->e_ident[EI_ABIVERSION]) {
    NaClLog(LOG_ERROR, "Expected ABIVERSION %d, got %d\n",
            EF_NACL_ABIVERSION,
            hdr->e_ident[EI_ABIVERSION]);
    if (abi_mismatch_option == NACL_ABI_MISMATCH_OPTION_ABORT) {
      return LOAD_BAD_ABI;
    }
  }
#else
  UNREFERENCED_PARAMETER(abi_mismatch_option);
#endif

  if (ET_EXEC != hdr->e_type) {
    return LOAD_NOT_EXEC;
  }

  if (EM_EXPECTED_BY_NACL != hdr->e_machine) {
    return LOAD_BAD_MACHINE;
  }

  if (EV_CURRENT != hdr->e_version) {
    return LOAD_BAD_ELF_VERS;
  }

  return LOAD_OK;
}


static void NaClDumpElfProgramHeader(Elf32_Phdr *phdr) {
#define DUMP(mem) do {                                         \
    NaClLog(2, "%s: %x\n", #mem, phdr->mem);      \
  } while (0)

  DUMP(p_type);
  DUMP(p_offset);
  DUMP(p_vaddr);
  DUMP(p_paddr);
  DUMP(p_filesz);
  DUMP(p_memsz);
  DUMP(p_flags);
  NaClLog(2, " (%s %s %s)\n",
          (phdr->p_flags & PF_R) ? "PF_R" : "",
          (phdr->p_flags & PF_W) ? "PF_W" : "",
          (phdr->p_flags & PF_X) ? "PF_X" : "");
  DUMP(p_align);
#undef  DUMP
  NaClLog(2, "\n");
}


NaClErrorCode NaClAppLoadFile(struct Gio                 *gp,
                              struct NaClApp             *nap,
                              enum NaClAbiMismatchOption abi_mismatch_option) {
  NaClErrorCode ret = LOAD_INTERNAL;
  NaClErrorCode subret;
  int           cur_ph;

  /* NACL_MAX_ADDR_BITS < 32 */
  if (nap->addr_bits > NACL_MAX_ADDR_BITS) {
    ret = LOAD_ADDR_SPACE_TOO_BIG;
    goto done;
  }

  nap->stack_size = NaClRoundAllocPage(nap->stack_size);

  /* nap->addr_bits <= NACL_MAX_ADDR_BITS < 32 */
  if ((*gp->vtbl->Read)(gp,
                        &nap->elf_hdr,
                        sizeof nap->elf_hdr)
      != sizeof nap->elf_hdr) {
    ret = LOAD_READ_ERROR;
    goto done;
  }

  NaClDumpElfHeader(&nap->elf_hdr);

  subret = NaClValidateElfHeader(&nap->elf_hdr, abi_mismatch_option);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  nap->entry_pt = nap->elf_hdr.e_entry;

  if (nap->elf_hdr.e_flags & EF_NACL_ALIGN_MASK) {
    unsigned long eflags = nap->elf_hdr.e_flags & EF_NACL_ALIGN_MASK;
    if (eflags == EF_NACL_ALIGN_16) {
      nap->align_boundary = 16;
    } else if (eflags == EF_NACL_ALIGN_32) {
      nap->align_boundary = 32;
    } else {
      ret = LOAD_BAD_ABI;
      goto done;
    }
  } else {
    nap->align_boundary = 32;
  }

  /* read program headers */
  if (nap->elf_hdr.e_phnum > NACL_MAX_PROGRAM_HEADERS) {
    ret = LOAD_TOO_MANY_SECT;  /* overloaded */
    goto done;
  }

  /* TODO(robertm): determine who allocated this */
  free(nap->phdrs);

  nap->phdrs = malloc(nap->elf_hdr.e_phnum * sizeof nap->phdrs[0]);
  if (!nap->phdrs) {
    ret = LOAD_NO_MEMORY;
    goto done;
  }
  if (nap->elf_hdr.e_phentsize < sizeof nap->phdrs[0]) {
    ret = LOAD_BAD_SECT;
    goto done;
  }
  for (cur_ph = 0; cur_ph < nap->elf_hdr.e_phnum; ++cur_ph) {
    if ((*gp->vtbl->Seek)(gp,
                          nap->elf_hdr.e_phoff
                          + cur_ph * nap->elf_hdr.e_phentsize,
                          SEEK_SET) == -1) {
      ret = LOAD_BAD_SECT;
      goto done;
    }
    if ((*gp->vtbl->Read)(gp,
                          &nap->phdrs[cur_ph],
                          sizeof nap->phdrs[0])
        != sizeof nap->phdrs[0]) {
      ret = LOAD_BAD_SECT;
      goto done;
    }


    NaClDumpElfProgramHeader(&nap->phdrs[cur_ph]);
  }

  /*
   * We need to determine the size of the CS region.  (The DS and SS
   * region sizes are obvious -- the entire application address
   * space.)  NaClProcessPhdrs will figure out nap->text_region_bytes.
   */

  subret = NaClProcessPhdrs(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  if (!NaClAddrIsValidEntryPt(nap, nap->entry_pt)) {
    ret = LOAD_BAD_ENTRY;
    goto done;
  }

  NaClLog(2, "Allocating address space\n");
  subret = NaClAllocAddrSpace(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  NaClLog(2, "Loading into memory\n");
  subret = NaClLoadImage(gp, nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

#if !defined(DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX)
  NaClLog(2, "Validating image\n");
  subret = NaClValidateImage(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }
#endif

  NaClLog(2, "Installing trampoline\n");

  NaClLoadTrampoline(nap);

  /*
   * This is necessary for ARM arch as we keep TLS pointer in register r9 or in
   * nacl_tls_idx varible. TLS hook is a piece of code patched into a trampoline
   * slot to retrieve the TLS pointer when needed by nacl module. The hook is
   * very simple piece of code, so we do not go to service runtime to perform
   * it.
   */
  NaClLoadTlsHook(nap);

  NaClLog(2, "Installing springboard\n");

  NaClLoadSpringboard(nap);

  NaClLog(2, "Applying memory protection\n");

  subret = NaClMemoryProtection(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  ret = LOAD_OK;
done:
  return ret;
}

int NaClAddrIsValidEntryPt(struct NaClApp *nap,
                           uintptr_t      addr) {
  if (0 != (addr & (nap->align_boundary - 1))) {
    return 0;
  }

  return addr < NACL_TRAMPOLINE_END + nap->text_region_bytes;
}

/*
 * preconditions:
 * argc > 0, argc and argv table is consistent
 * envv may be NULL (this happens on MacOS/Cocoa
 * if envv is non-NULL it is 'consistent', null terminated etc.
 */
int NaClCreateMainThread(struct NaClApp     *nap,
                         int                argc,
                         char               **argv,
                         char const *const  *envv) {
  /*
   * Compute size of string tables for argv and envv
   */
  int                   retval;
  int                   envc;
  char const *const     *pp;
  size_t                size;
  int                   i;
  char                  *p;
  char                  *strp;
  int                   *argv_len;
  int                   *envv_len;
  struct NaClAppThread  *natp;
  uintptr_t             stack_ptr;

  /*
   * Set an exception handler so Windows won't show a message box if
   * an exception occurs
   */
  WINDOWS_EXCEPTION_TRY;

  retval = 0;  /* fail */
  CHECK(argc > 0);
  CHECK(NULL != argv);

  if (envv == NULL) {
    envc = 0;
  } else {
    for (pp = envv, envc = 0; NULL != *pp; ++pp, ++envc)
      ;
  }
  envv_len = 0;
  argv_len = malloc(argc * sizeof argv_len[0]);
  envv_len = malloc(envc * sizeof envv_len[0]);
  if (NULL == argv_len) {
    goto cleanup;
  }
  if (NULL == envv_len && 0 != envc) {
    goto cleanup;
  }

  size = 0;

  for (i = 0; i < argc; ++i) {
    argv_len[i] = strlen(argv[i]) + 1;
    size += argv_len[i];
  }
  for (i = 0; i < envc; ++i) {
    envv_len[i] = strlen(envv[i]) + 1;
    size += envv_len[i];
  }

  size += (argc + envc + 4) * sizeof(char *) + sizeof(int);

  size = (size + PTR_ALIGN_MASK) & ~PTR_ALIGN_MASK;

  if (size > nap->stack_size) {
    retval = 0;
    goto cleanup;
  }

  /* write strings and char * arrays to stack */

  stack_ptr = (nap->mem_start + (1 << nap->addr_bits) - size);
  VCHECK(0 == (stack_ptr & PTR_ALIGN_MASK),
          ("stack_ptr not aligned: %08x\n", stack_ptr));

  p = (char *) stack_ptr;
  strp = p + (argc + envc + 4) * sizeof(char *) + sizeof(int);

#define BLAT(t, v) do { \
    *(t *) p = (t) v; p += sizeof(t);           \
  } while (0);

  BLAT(int, argc);

  for (i = 0; i < argc; ++i) {
    BLAT(char *, NaClSysToUser(nap, (uintptr_t) strp));
    strcpy(strp, argv[i]);
    strp += argv_len[i];
  }
  BLAT(char *, 0);

  for (i = 0; i < envc; ++i) {
    BLAT(char *, NaClSysToUser(nap, (uintptr_t) strp));
    strcpy(strp, envv[i]);
    strp += envv_len[i];
  }
  BLAT(char *, 0);
  /* Push an empty auxv for glibc support */
  BLAT(char *, 0);
  BLAT(char *, 0);
#undef BLAT

  /* now actually spawn the thread */
  natp = malloc(sizeof *natp);
  if (!natp) {
    goto cleanup;
  }

  nap->running = 1;

  /* e_entry is user addr */
  if (!NaClAppThreadAllocSegCtor(natp,
                                 nap,
                                 1,
                                 nap->elf_hdr.e_entry,
                                 NaClSysToUser(nap, stack_ptr),
                                 NaClUserToSys(nap, nap->break_addr),
                                 1)) {
    retval = 0;
    goto cleanup;
  }

  /*
   * NB: Hereafter locking is required to access nap.
   */
  retval = 1;
cleanup:
  free(argv_len);
  free(envv_len);

  WINDOWS_EXCEPTION_CATCH;
  return retval;
}

int NaClWaitForMainThreadToExit(struct NaClApp  *nap) {
  struct NaClClosure        *work;

  while (NULL != (work = NaClSyncQueueDequeue(&nap->work_queue))) {
    NaClLog(3, "NaClWaitForMainThreadToExit: got work %08"PRIxPTR"\n",
            (uintptr_t) work);
    NaClLog(3, " invoking Run fn %08"PRIxPTR"\n",
            (uintptr_t) work->vtbl->Run);

    (*work->vtbl->Run)(work);
    NaClLog(3, "... done\n");
  }

  NaClLog(3, " taking NaClApp lock\n");
  NaClXMutexLock(&nap->mu);
  NaClLog(3, " waiting for exit status\n");
  while (nap->running) {
    NaClCondVarWait(&nap->cv, &nap->mu);
  }
  /*
   * Some thread invoked the exit (exit_group) syscall.
   */

  return (nap->exit_status);
}

int32_t NaClCreateAdditionalThread(struct NaClApp *nap,
                                   uintptr_t      prog_ctr,
                                   uintptr_t      stack_ptr,
                                   uintptr_t      sys_tdb,
                                   size_t         tdb_size) {
  struct NaClAppThread  *natp;

  natp = malloc(sizeof *natp);
  if (NULL == natp) {
    return -NACL_ABI_ENOMEM;
  }
  if (!NaClAppThreadAllocSegCtor(natp,
                                 nap,
                                 0,
                                 prog_ctr,
                                 stack_ptr,
                                 sys_tdb,
                                 tdb_size)) {
    return -NACL_ABI_ENOMEM;
  }
  return 0;
}
