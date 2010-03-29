/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_effector_cleanup.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/trusted/handle_pass/ldr_handle.h"

#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/gio_shm.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

int NaClAppCtor(struct NaClApp  *nap) {

  nap->addr_bits = NACL_MAX_ADDR_BITS;

  nap->max_data_alloc = NACL_DEFAULT_ALLOC_MAX;
  nap->stack_size = NACL_DEFAULT_STACK_MAX;

  nap->mem_start = 0;
  nap->guard_pages_initialized = 0;
  nap->static_text_end = 0;
  nap->dynamic_text_start = 0;
  nap->dynamic_text_end = 0;
  nap->rodata_start = 0;
  nap->data_start = 0;
  nap->data_end = 0;

  nap->entry_pt = 0;

  if (!DynArrayCtor(&nap->threads, 2)) {
    goto cleanup_none;
  }
  if (!DynArrayCtor(&nap->desc_tbl, 2)) {
    goto cleanup_threads;
  }
  if (!NaClVmmapCtor(&nap->mem_map)) {
    goto cleanup_desc_tbl;
  }

  nap->use_shm_for_dynamic_text = 0;
  nap->text_shm = NULL;

  nap->service_port = NULL;
  nap->service_address = NULL;
  nap->secure_channel = NULL;

  if (!NaClMutexCtor(&nap->mu)) {
    goto cleanup_mem_map;
  }
  if (!NaClCondVarCtor(&nap->cv)) {
    goto cleanup_mu;
  }

  nap->origin = (char *) NULL;
  nap->module_load_status = LOAD_STATUS_UNKNOWN;
  nap->module_may_start = 0;  /* only when secure_channel != NULL */

  nap->restrict_to_main_thread = 1;

  if (!NaClSyncQueueCtor(&nap->work_queue)) {
    goto cleanup_cv;
  }

  if (!NaClMutexCtor(&nap->threads_mu)) {
    goto cleanup_work_queue;
  }
  if (!NaClCondVarCtor(&nap->threads_cv)) {
    goto cleanup_threads_mu;
  }
  nap->num_threads = 0;
  if (!NaClMutexCtor(&nap->desc_mu)) {
    goto cleanup_threads_cv;
  }

  nap->running = 0;
  nap->exit_status = -1;

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  nap->code_seg_sel = 0;
  nap->data_seg_sel = 0;
#endif

  nap->freeze_thread_ops = 0;

  return 1;

#if 0
cleanup_desc_mu:
  NaClMutexDtor(&nap->desc_mu);
#endif
cleanup_threads_cv:
  NaClCondVarDtor(&nap->threads_cv);
cleanup_threads_mu:
  NaClMutexDtor(&nap->threads_mu);
cleanup_work_queue:
  NaClSyncQueueDtor(&nap->work_queue);
cleanup_cv:
  NaClCondVarDtor(&nap->cv);
cleanup_mu:
  NaClMutexDtor(&nap->mu);
cleanup_mem_map:
  NaClVmmapDtor(&nap->mem_map);
cleanup_desc_tbl:
  DynArrayDtor(&nap->desc_tbl);
cleanup_threads:
  DynArrayDtor(&nap->threads);
cleanup_none:
  return 0;
}

void NaClAppDtor(struct NaClApp  *nap) {
  size_t                i;
  struct NaClDesc       *ndp;
  struct NaClAppThread  *natp;

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassLdrDtor();
#endif

  NaClLog(2,
          ("NaClAppDtor: there are %d threads alive;"
           " thread table size %"NACL_PRIuS"\n"),
          nap->num_threads,
          nap->threads.num_entries);
  for (i = 0; i < nap->threads.num_entries; ++i) {
    int                   refcount;
    enum NaClThreadState  state;

    NaClLog(2, "Checking thread %"NACL_PRIuS"\n", i);
    if (NULL == (natp = NaClGetThreadMu(nap, (int) i))) {
      continue;
    }
    NaClLog(2, "Extracting state for thread %"NACL_PRIuS"\n", i);
    NaClXMutexLock(&natp->mu);
    state = natp->state;
    NaClLog(2, "state %d\n", state);
    NaClXMutexUnlock(&natp->mu);

    NaClRemoveThreadMu(nap, (int) i);
    refcount = NaClAppThreadDecRef(natp);

    if (state != NACL_APP_THREAD_DEAD) {
      NaClLog(LOG_WARNING,
              ("NaClAppDtor: thread %"NACL_PRIuS" still running when NaCl app"
               " is being destroyed?!?\n"),
              i);
    }
    if (refcount != 0) {
      NaClLog(LOG_WARNING,
              ("NaClAppDtor: thread %"NACL_PRIuS" refcount not 0 when NaCl app"
               " is being destroyed?!?\n"),
              i);
    }
  }

  NaClLog(4, "There are %"NACL_PRIuS" descriptor entries\n",
          nap->desc_tbl.num_entries);

  for (i = 0; i < nap->desc_tbl.num_entries; ++i) {
    ndp = (struct NaClDesc *) DynArrayGet(&nap->desc_tbl, i);
    NaClDescSafeUnref(ndp);
  }

  NaClLog(4,
          "Deallocating synchronization variables for"
          " desc, thread, work_queue\n");

  NaClMutexDtor(&nap->desc_mu);
  NaClMutexDtor(&nap->threads_mu);
  NaClCondVarDtor(&nap->threads_cv);
  NaClSyncQueueDtor(&nap->work_queue);

  NaClLog(4, "Freeing NaCl module origin\n");

  free(nap->origin);
  nap->origin = (char *) NULL;

  NaClLog(4, "Freeing text_shm\n");

  NaClDescSafeUnref(nap->text_shm);
  nap->text_shm = NULL;

  NaClLog(4, "Freeing service_port\n");

  NaClDescSafeUnref(nap->service_port);
  nap->service_port = NULL;

  NaClLog(4, "Freeing service_address\n");

  NaClDescSafeUnref(nap->service_address);
  nap->service_address = NULL;

  NaClLog(4, "Freeing secure channel\n");

  NaClDescSafeUnref(nap->secure_channel);
  nap->secure_channel = NULL;

  NaClLog(4, "Freeing synchronization variables for the NaClApp\n");

  NaClCondVarDtor(&nap->cv);
  NaClMutexDtor(&nap->mu);

  NaClLog(4, "Freeing memory\n");

  NaClAppFreeAllMemory(nap);

  NaClLog(4, "Freeing vmmap\n");

  NaClVmmapDtor(&nap->mem_map);

  NaClLog(4, "Freeing desc_tbl, threads\n");

  DynArrayDtor(&nap->desc_tbl);
  DynArrayDtor(&nap->threads);

  NaClLog(4, "NaClAppDtor: Done\n");

  return;
}

size_t  NaClAlignPad(size_t val, size_t align) {
  /* align is always a power of 2, but we do not depend on it */
  if (!align) {
    NaClLog(0,
            "sel_ldr: NaClAlignPad, align == 0, at 0x%08"NACL_PRIxS"\n",
            val);
    return 0;
  }
  val = val % align;
  if (!val) return 0;
  return align - val;
}

/*
 * unaligned little-endian load.  precondition: nbytes should never be
 * more than 8.
 */
static uint64_t NaClLoadMem(uintptr_t addr,
                            size_t    user_nbytes) {
  uint64_t      value = 0;

  CHECK(0 != user_nbytes && user_nbytes <= 8);

  do {
    value = value << 8;
    value |= ((uint8_t *) addr)[--user_nbytes];
  } while (user_nbytes > 0);
  return value;
}

#define GENERIC_LOAD(bits) \
  static uint ## bits ## _t NaClLoad ## bits(uintptr_t addr) { \
    return (uint ## bits ## _t) NaClLoadMem(addr, sizeof(uint ## bits ## _t)); \
  }

#if NACL_TARGET_SUBARCH == 32
GENERIC_LOAD(32)
#endif
GENERIC_LOAD(64)

#undef GENERIC_LOAD

/*
 * unaligned little-endian store
 */
static void NaClStoreMem(uintptr_t  addr,
                         size_t     nbytes,
                         uint64_t   value) {
  size_t i;

  CHECK(nbytes <= 8);

  for (i = 0; i < nbytes; ++i) {
    ((uint8_t *) addr)[i] = (uint8_t) value;
    value = value >> 8;
  }
}

#define GENERIC_STORE(bits) \
  static void NaClStore ## bits(uintptr_t addr, \
                                uint ## bits ## _t v) { \
    NaClStoreMem(addr, sizeof(uint ## bits ## _t), v); \
  }

GENERIC_STORE(16)
GENERIC_STORE(32)
GENERIC_STORE(64)

#undef GENERIC_STORE

struct NaClPatchInfo *NaClPatchInfoCtor(struct NaClPatchInfo *self) {
  if (NULL != self) {
    memset(self, 0, sizeof *self);
  }
  return self;
}

/*
 * This function is called by NaClLoadTrampoline and NaClLoadSpringboard to
 * patch a single memory location specified in NaClPatchInfo structure.
 */
void  NaClApplyPatchToMemory(struct NaClPatchInfo  *patch) {
  size_t    i;
  size_t    offset;
  int64_t   reloc;
  uintptr_t target_addr;

  memcpy((void *) patch->dst, (void *) patch->src, patch->nbytes);

  reloc = patch->dst - patch->src;


  for (i = 0; i < patch->num_abs64; ++i) {
    offset = patch->abs64[i].target - patch->src;
    target_addr = patch->dst + offset;
    NaClStore64(target_addr, patch->abs64[i].value);
  }

  for (i = 0; i < patch->num_abs32; ++i) {
    offset = patch->abs32[i].target - patch->src;
    target_addr = patch->dst + offset;
    NaClStore32(target_addr, (uint32_t) patch->abs32[i].value);
  }

  for (i = 0; i < patch->num_abs16; ++i) {
    offset = patch->abs16[i].target - patch->src;
    target_addr = patch->dst + offset;
    NaClStore16(target_addr, (uint16_t) patch->abs16[i].value);
  }

  for (i = 0; i < patch->num_rel64; ++i) {
    offset = patch->rel64[i] - patch->src;
    target_addr = patch->dst + offset;
    NaClStore64(target_addr, NaClLoad64(target_addr) - reloc);
  }

  /*
   * rel32 is only supported on 32-bit architectures. The range of a relative
   * relocation in untrusted space is +/- 4GB. This can be represented as
   * an unsigned 32-bit value mod 2^32, which is handy on a 32 bit system since
   * all 32-bit pointer arithmetic is implicitly mod 2^32. On a 64 bit system,
   * however, pointer arithmetic is implicitly modulo 2^64, which isn't as
   * helpful for our purposes. We could simulate the 32-bit behavior by
   * explicitly modding all relative addresses by 2^32, but that seems like an
   * expensive way to save a few bytes per reloc.
   */
#if NACL_TARGET_SUBARCH == 32
  for (i = 0; i < patch->num_rel32; ++i) {
    offset = patch->rel32[i] - patch->src;
    target_addr = patch->dst + offset;
    NaClStore32(target_addr,
      (uint32_t) NaClLoad32(target_addr) - (int32_t) reloc);
  }
#endif
}


/*
 * Install syscall trampolines at all possible well-formed entry
 * points within the trampoline pages.  Many of these syscalls will
 * correspond to unimplemented system calls and will just abort the
 * program.
 */
void  NaClLoadTrampoline(struct NaClApp *nap) {
  int         num_syscalls;
  int         i;
  uintptr_t   addr;

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32 && __PIC__
  if (!NaClMakePcrelThunk(nap)) {
    NaClLog(LOG_FATAL, "NaClMakePcrelThunk failed!\n");
  }
#endif
  NaClFillTrampolineRegion(nap);

  /*
   * Do not bother to fill in the contents of page 0, since we make it
   * inaccessible later (see sel_addrspace.c, NaClMemoryProtection)
   * anyway to help detect NULL pointer errors, and we might as well
   * not dirty the page.
   *
   * The last syscall entry point is used for springboard code.
   */
  num_syscalls = ((NACL_TRAMPOLINE_END - NACL_SYSCALL_START_ADDR)
                  / NACL_SYSCALL_BLOCK_SIZE) - 1;

  NaClLog(2, "num_syscalls = %d (0x%x)\n", num_syscalls, num_syscalls);

  for (i = 0, addr = nap->mem_start + NACL_SYSCALL_START_ADDR;
       i < num_syscalls;
       ++i, addr += NACL_SYSCALL_BLOCK_SIZE) {
    NaClPatchOneTrampoline(nap, addr);
  }
}

void  NaClMemRegionPrinter(void                   *state,
                           struct NaClVmmapEntry  *entry) {
  struct Gio *gp = (struct Gio *) state;

  gprintf(gp, "\nPage   %"NACL_PRIdPTR" (0x%"NACL_PRIxPTR")\n",
          entry->page_num, entry->page_num);
  gprintf(gp,   "npages %"NACL_PRIdS" (0x%"NACL_PRIxS")\n", entry->npages,
          entry->npages);
  gprintf(gp,   "prot   0x%08x\n", entry->prot);
  gprintf(gp,   "%sshared/backed by a file\n",
          (NULL == entry->nmop) ? "not " : "");
}

void  NaClAppPrintDetails(struct NaClApp  *nap,
                          struct Gio      *gp) {
  NaClXMutexLock(&nap->mu);
  gprintf(gp,
          "NaClAppPrintDetails((struct NaClApp *) 0x%08"NACL_PRIxPTR","
          "(struct Gio *) 0x%08"NACL_PRIxPTR")\n", (uintptr_t) nap,
          (uintptr_t) gp);
  gprintf(gp, "addr space size:  2**%"NACL_PRId32"\n", nap->addr_bits);
  gprintf(gp, "max data alloc:   0x%08"NACL_PRIx32"\n", nap->max_data_alloc);
  gprintf(gp, "stack size:       0x%08"NACL_PRIx32"\n", nap->stack_size);

  gprintf(gp, "mem start addr:   0x%08"NACL_PRIxPTR"\n", nap->mem_start);
  /*           123456789012345678901234567890 */

  gprintf(gp, "static_text_end:   0x%08"NACL_PRIxPTR"\n", nap->static_text_end);
  gprintf(gp, "end-of-text:       0x%08"NACL_PRIxPTR"\n",
          NaClEndOfStaticText(nap));
  gprintf(gp, "rodata:            0x%08"NACL_PRIxPTR"\n", nap->rodata_start);
  gprintf(gp, "data:              0x%08"NACL_PRIxPTR"\n", nap->data_start);
  gprintf(gp, "data_end:          0x%08"NACL_PRIxPTR"\n", nap->data_end);
  gprintf(gp, "break_addr:        0x%08"NACL_PRIxPTR"\n", nap->break_addr);

  gprintf(gp, "ELF entry point:  0x%08x\n", nap->entry_pt);
  gprintf(gp, "memory map:\n");
  NaClVmmapVisit(&nap->mem_map,
                 NaClMemRegionPrinter,
                 gp);
  NaClXMutexUnlock(&nap->mu);
}

/* deprecated */
char const  *NaClAppLoadErrorString(NaClErrorCode errcode) {
  NaClLog(0, "Deprecated NaClAppLoadErrorString interface used\n");
  return NaClErrorString(errcode);
}

char const  *NaClErrorString(NaClErrorCode errcode) {
  switch (errcode) {
    case LOAD_OK:
      return "Ok";
    case LOAD_STATUS_UNKNOWN:
      return "Load status unknown (load incomplete)";
    case LOAD_UNSUPPORTED_OS_PLATFORM:
      return "Operating system platform is not supported";
    case LOAD_INTERNAL:
      return "Internal error";
    case LOAD_READ_ERROR:
      return "Cannot read file";
    case LOAD_TOO_MANY_PROG_HDRS:
      return "Too many program header entries in ELF file";
    case LOAD_PROG_HDR_SIZE_TOO_SMALL:
      return "ELF program header size too small";
    case LOAD_BAD_ELF_MAGIC:
      return "Bad ELF header magic number";
    case LOAD_NOT_32_BIT:
      return "Not a 32-bit ELF file";
    case LOAD_NOT_64_BIT:
      return "Not a 64-bit ELF file";
    case LOAD_BAD_ABI:
      return "ELF file has unexpected OS ABI";
    case LOAD_NOT_EXEC:
      return "ELF file type not executable";
    case LOAD_BAD_MACHINE:
      return "ELF file for wrong architecture";
    case LOAD_BAD_ELF_VERS:
      return "ELF version mismatch";
    case LOAD_TOO_MANY_SECT:
      return "Too many section headers";
    case LOAD_BAD_SECT:
      return "ELF bad sections";
    case LOAD_NO_MEMORY:
      return "Insufficient memory to load file";
    case LOAD_SECT_HDR:
      return "ELF section header string table load error";
    case LOAD_ADDR_SPACE_TOO_SMALL:
      return "Address space too small";
    case LOAD_ADDR_SPACE_TOO_BIG:
      return "Address space too big";
    case LOAD_DATA_OVERLAPS_STACK_SECTION:
      return ("Memory \"hole\" between end of BSS and start of stack"
              " is negative in size");
    case LOAD_RODATA_OVERLAPS_DATA:
      return "Read-only data segment overlaps data segment";
    case LOAD_DATA_NOT_LAST_SEGMENT:
      return "Data segment exists, but is not last segment";
    case LOAD_NO_DATA_BUT_RODATA_NOT_LAST_SEGMENT:
      return ("No data segment, read-only data segment exists,"
              " but is not last segment");
    case LOAD_TEXT_OVERLAPS_RODATA:
      return "Text segment overlaps rodata segment";
    case LOAD_TEXT_OVERLAPS_DATA:
      return "No rodata segment, and text segment overlaps data segment";
    case LOAD_UNLOADABLE:
      return "Error during loading";
    case LOAD_BAD_ELF_TEXT:
      return "ELF file contains no text segment";
    case LOAD_TEXT_SEG_TOO_BIG:
      return "ELF file text segment too large";
    case LOAD_DATA_SEG_TOO_BIG:
      return "ELF file data segment(s) too large";
    case LOAD_MPROTECT_FAIL:
      return "Cannot protect pages";
    case LOAD_MADVISE_FAIL:
      return "Cannot release unused data segment";
    case LOAD_TOO_MANY_SYMBOL_STR:
      return "Malformed ELF file: too many string tables";
    case LOAD_SYMTAB_ENTRY_TOO_SMALL:
      return "Symbol table entry too small";
    case LOAD_NO_SYMTAB:
      return "No symbol table";
    case LOAD_NO_SYMTAB_STRINGS:
      return "No string table for symbols";
    case LOAD_SYMTAB_ENTRY:
      return "Error entering new symbol into symbol table";
    case LOAD_UNKNOWN_SYMBOL_TYPE:
      return "Unknown symbol type";
    case LOAD_SYMTAB_DUP:
      return "Duplicate entry in symbol table";
    case LOAD_REL_ERROR:
      return "Bad relocation read error";
    case LOAD_REL_UNIMPL:
      return "Relocation type unimplemented";
    case LOAD_UNDEF_SYMBOL:
      return "Undefined external symbol";
    case LOAD_BAD_SYMBOL_DATA:
      return "Bad symbol table data";
    case LOAD_BAD_FILE:
      return "ELF file not accessible";
    case LOAD_BAD_ENTRY:
      return "Bad program entry point address";
    case LOAD_SEGMENT_OUTSIDE_ADDRSPACE:
      return ("ELF executable contains a segment which lies outside"
              " the assigned address space");
    case LOAD_DUP_SEGMENT:
      return ("ELF executable contains a duplicate segment"
              " (please run objdump to see which)");
    case LOAD_SEGMENT_BAD_LOC:
      return "ELF executable text/rodata segment has wrong starting address";
    case LOAD_BAD_SEGMENT:
      return "ELF executable contains an unexpected/unallowed segment/flags";
    case LOAD_REQUIRED_SEG_MISSING:
      return "ELF executable missing a required segment (text)";
    case LOAD_SEGMENT_BAD_PARAM:
      return "ELF executable segment header parameter error";
    case LOAD_VALIDATION_FAILED:
      return "Validation failure. File violates Native Client safety rules.";
    case SRT_NO_SEG_SEL:
      return "Service Runtime: cannot allocate segment selector";
  }

  /*
   * do not use default case label, to make sure that the compiler
   * will generate a warning with -Wswitch-enum for new codes
   * introduced in nacl_error_codes.h for which there is no
   * corresponding entry here.  instead, we pretend that fall-through
   * from the switch is possible.  (otherwise -W complains control
   * reaches end of non-void function.)
   */
  return "BAD ERROR CODE";
}

struct NaClDesc *NaClGetDescMu(struct NaClApp *nap,
                               int            d) {
  struct NaClDesc *result;

  result = (struct NaClDesc *) DynArrayGet(&nap->desc_tbl, d);
  if (NULL != result) {
    NaClDescRef(result);
  }

  return result;
}

void NaClSetDescMu(struct NaClApp   *nap,
                   int              d,
                   struct NaClDesc  *ndp) {
  struct NaClDesc *result;

  result = (struct NaClDesc *) DynArrayGet(&nap->desc_tbl, d);
  NaClDescSafeUnref(result);

  if (!DynArraySet(&nap->desc_tbl, d, ndp)) {
    NaClLog(LOG_FATAL,
            "NaClSetDesc: could not set descriptor %d to 0x%08"
            NACL_PRIxPTR"\n",
            d,
            (uintptr_t) ndp);
  }
}

int32_t NaClSetAvailMu(struct NaClApp  *nap,
                       struct NaClDesc *ndp) {
  size_t pos;

  pos = DynArrayFirstAvail(&nap->desc_tbl);

  if (pos > INT32_MAX) {
    NaClLog(LOG_FATAL,
            ("NaClSetAvailMu: DynArrayFirstAvail returned a value"
             " that is greather than 2**31-1.\n"));
  }

  NaClSetDescMu(nap, (int) pos, ndp);

  return (int32_t) pos;
}

struct NaClDesc *NaClGetDesc(struct NaClApp *nap,
                             int            d) {
  struct NaClDesc *res;

  NaClXMutexLock(&nap->desc_mu);
  res = NaClGetDescMu(nap, d);
  NaClXMutexUnlock(&nap->desc_mu);
  return res;
}

void NaClSetDesc(struct NaClApp   *nap,
                 int              d,
                 struct NaClDesc  *ndp) {
  NaClXMutexLock(&nap->desc_mu);
  NaClSetDescMu(nap, d, ndp);
  NaClXMutexUnlock(&nap->desc_mu);
}

int32_t NaClSetAvail(struct NaClApp  *nap,
                 struct NaClDesc *ndp) {
  int32_t pos;

  NaClXMutexLock(&nap->desc_mu);
  pos = NaClSetAvailMu(nap, ndp);
  NaClXMutexUnlock(&nap->desc_mu);

  return pos;
}

int NaClAddThreadMu(struct NaClApp        *nap,
                    struct NaClAppThread  *natp) {
  size_t pos;

  pos = DynArrayFirstAvail(&nap->threads);

  if (!DynArraySet(&nap->threads, pos, natp)) {
    NaClLog(LOG_FATAL,
            "NaClAddThreadMu: DynArraySet at position %"NACL_PRIuS" failed\n",
            pos);
  }
  ++nap->num_threads;
  return (int) pos;
}

int NaClAddThread(struct NaClApp        *nap,
                  struct NaClAppThread  *natp) {
  int pos;

  NaClXMutexLock(&nap->threads_mu);
  pos = NaClAddThreadMu(nap, natp);
  NaClXMutexUnlock(&nap->threads_mu);

  return pos;
}

void NaClRemoveThreadMu(struct NaClApp  *nap,
                        int             thread_num) {
  if (NULL == DynArrayGet(&nap->threads, thread_num)) {
    NaClLog(LOG_FATAL,
            "NaClRemoveThreadMu:: thread to be removed is not in the table\n");
  }
  if (nap->num_threads == 0) {
    NaClLog(LOG_FATAL,
            "NaClRemoveThreadMu:: num_threads cannot be 0!!!\n");
  }
  --nap->num_threads;
  if (!DynArraySet(&nap->threads, thread_num, (struct NaClAppThread *) NULL)) {
    NaClLog(LOG_FATAL,
            "NaClRemoveThreadMu:: DynArraySet at position %d failed\n",
            thread_num);
  }
}

void NaClRemoveThread(struct NaClApp  *nap,
                      int             thread_num) {
  NaClXMutexLock(&nap->threads_mu);
  NaClRemoveThreadMu(nap, thread_num);
  NaClXCondVarBroadcast(&nap->threads_cv);
  NaClXMutexUnlock(&nap->threads_mu);
}

struct NaClAppThread *NaClGetThreadMu(struct NaClApp  *nap,
                                      int             thread_num) {
  return (struct NaClAppThread *) DynArrayGet(&nap->threads, thread_num);
}

void NaClAddHostDescriptor(struct NaClApp *nap,
                           int            host_os_desc,
                           int            mode,
                           int            nacl_desc) {
  struct NaClDescIoDesc *dp;

  dp = NaClDescIoDescMake(NaClHostDescPosixMake(host_os_desc, mode));
  NaClSetDesc(nap, nacl_desc, (struct NaClDesc *) dp);
}

void NaClAddImcHandle(struct NaClApp  *nap,
                      NaClHandle      h,
                      int             nacl_desc) {
  struct NaClDescImcDesc  *dp;

  dp = malloc(sizeof *dp);
  if (NULL == dp) {
    NaClLog(LOG_FATAL, "NaClAddImcHandle: no memory\n");
  }
  if (!NaClDescImcDescCtor(dp, h)) {
    NaClLog(LOG_FATAL, ("NaClAddImcHandle: cannot construct"
                        " IMC descriptor object\n"));
  }
  NaClSetDesc(nap, nacl_desc, (struct NaClDesc *) dp);
}

void NaClAddImcAddr(struct NaClApp                  *nap,
                    struct NaClSocketAddress const  *addr,
                    int                             nacl_desc) {
  struct NaClDescConnCap  *dp;

  dp = malloc(sizeof *dp);
  if (NULL == dp) {
    NaClLog(LOG_FATAL, "NaClAddImcAddr: no memory\n");
  }
  if (!NaClDescConnCapCtor(dp, addr)) {
    NaClLog(LOG_FATAL, ("NaClAddImcAddr: canot construct"
                        " connection capability object\n"));
  }
  NaClSetDesc(nap, nacl_desc, (struct NaClDesc *) dp);
}

void NaClAppVmmapUpdate(struct NaClApp    *nap,
                        uintptr_t         page_num,
                        size_t            npages,
                        int               prot,
                        struct NaClMemObj *nmop,
                        int               remove) {
  NaClXMutexLock(&nap->mu);
  NaClVmmapUpdate(&nap->mem_map,
                  page_num,
                  npages,
                  prot,
                  nmop,
                  remove);
  NaClXMutexUnlock(&nap->mu);
}

uintptr_t NaClAppVmmapFindSpace(struct NaClApp  *nap,
                                int             num_pages) {
  uintptr_t rv;

  NaClXMutexLock(&nap->mu);
  rv = NaClVmmapFindSpace(&nap->mem_map,
                          num_pages);
  NaClXMutexUnlock(&nap->mu);
  return rv;
}

uintptr_t NaClAppVmmapFindMapSpace(struct NaClApp *nap,
                                   int            num_pages) {
  uintptr_t rv;

  NaClXMutexLock(&nap->mu);
  rv = NaClVmmapFindMapSpace(&nap->mem_map,
                             num_pages);
  NaClXMutexUnlock(&nap->mu);
  return rv;
}

void NaClCreateServiceSocket(struct NaClApp *nap) {
  struct NaClDesc *pair[2];

  NaClLog(3, "Entered NaClCreateServiceSocket\n");
  if (0 != NaClCommonDescMakeBoundSock(pair)) {
    NaClLog(LOG_FATAL, "Cound not create service socket\n");
  }
  NaClLog(4,
          "got bound socket at 0x%08"NACL_PRIxPTR", "
          "addr at 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) pair[0],
          (uintptr_t) pair[1]);
  NaClSetDesc(nap, NACL_SERVICE_PORT_DESCRIPTOR, pair[0]);
  NaClSetDesc(nap, NACL_SERVICE_ADDRESS_DESCRIPTOR, pair[1]);

  NaClDescSafeUnref(nap->service_port);

  nap->service_port = pair[0];
  NaClDescRef(nap->service_port);

  NaClDescSafeUnref(nap->service_address);

  nap->service_address = pair[1];
  NaClDescRef(nap->service_address);
  NaClLog(4, "Leaving NaClCreateServiceSocket\n");
}

void NaClSendServiceAddressTo(struct NaClApp  *nap,
                              int             desc) {
  struct NaClDesc             *channel;
  struct NaClImcTypedMsgHdr   hdr;
  ssize_t                     rv;

  struct NaClNrdXferEffector  nnxep;

  NaClLog(4,
          "NaClSendServiceAddressTo(0x%08"NACL_PRIxPTR", %d)\n",
          (uintptr_t) nap,
          desc);

  channel = NaClGetDesc(nap, desc);
  if (NULL == channel) {
    NaClLog(LOG_FATAL,
            "NaClSendServiceAddressTo: descriptor %d not in open file table\n",
            desc);
    return;
  }
  if (NULL == nap->service_address) {
    NaClLog(LOG_FATAL,
            "NaClSendServiceAddressTo: service address not set\n");
    return;
  }
  /*
   * service_address and service_port are set together.
   */
  (void) NaClNrdXferEffectorCtor(&nnxep, nap->service_port);

  hdr.iov = (struct NaClImcMsgIoVec *) NULL;
  hdr.iov_length = 0;
  hdr.ndescv = &nap->service_address;
  hdr.ndesc_length = 1;

  rv = NaClImcSendTypedMessage(channel,
                               &nnxep.base,
                               &hdr, 0);

  NaClDescUnref(channel);
  channel = NULL;

  (*nnxep.base.vtbl->Dtor)(&nnxep.base);

  NaClLog(1,
          "NaClSendServiceAddressTo: descriptor %d, error %"NACL_PRIdS"\n",
          desc,
          rv);
}

static NaClSrpcError NaClSecureChannelShutdownRpc(
    struct NaClSrpcChannel  *chan,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args) {
  UNREFERENCED_PARAMETER(chan);
  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(4, "NaClSecureChannelShutdownRpc (hard_shutdown), exiting\n");
  _exit(0);
}

/*
 * This RPC is invoked by the plugin when the nexe is downloaded as a
 * stream and not as a file. The only arguments are a handle to a
 * shared memory object that contains the nexe.
 */
static NaClSrpcError NaClLoadModuleRpc(struct NaClSrpcChannel  *chan,
                                       struct NaClSrpcArg      **in_args,
                                       struct NaClSrpcArg      **out_args) {
  struct NaClApp        *nap = (struct NaClApp *) chan->server_instance_data;
  struct NaClDesc       *nexe_binary = in_args[0]->u.hval;
  struct NaClGioShm     gio_shm;
  struct nacl_abi_stat  stbuf;
  int                   rval;
  NaClErrorCode         suberr;
  NaClErrorCode         errcode;
  size_t                rounded_size;

  UNREFERENCED_PARAMETER(out_args);

  NaClLog(4, "NaClLoadModuleRpc: entered, finding shm size\n");

  errcode = NACL_SRPC_RESULT_INTERNAL;

  /*
   * We don't know the actual size of the nexe, but it should not
   * matter.  The shared memory object's size is rounded up to at
   * least 4K, and we can map it in with uninitialized data (should be
   * zero filled) at the end.
   */
  rval = (*nexe_binary->vtbl->Fstat)(nexe_binary,
                                     &chan->eff.base,
                                     &stbuf);
  if (0 != rval) {
    goto cleanup;
  }

  rounded_size = (size_t) stbuf.nacl_abi_st_size;

  NaClLog(4, "NaClLoadModuleRpc: shm size 0x%"NACL_PRIxS"\n", rounded_size);

  if (!NaClGioShmCtor(&gio_shm, nexe_binary, rounded_size)) {
    errcode = NACL_SRPC_RESULT_NO_MEMORY;
    goto cleanup;
  }
  /* henceforth gio_shm must be dtor'd */

  suberr = NaClAppLoadFile((struct Gio *) &gio_shm,
                           nap,
                           NACL_ABI_CHECK_OPTION_CHECK);
  (*gio_shm.base.vtbl->Close)(&gio_shm.base);
  (*gio_shm.base.vtbl->Dtor)(&gio_shm.base);

  if (LOAD_OK != suberr) {
    nap->module_load_status = suberr;
    errcode = NACL_SRPC_RESULT_APP_ERROR;
    goto cleanup;
  }
  /*
   * Finish setting up the NaCl App.  This includes dup'ing
   * descriptors 0-2 and making them available to the NaCl App.
   */
  suberr = NaClAppPrepareToLaunch(nap, 0, 1, 2);
  if (LOAD_OK != suberr) {
    nap->module_load_status = suberr;
    errcode = NACL_SRPC_RESULT_APP_ERROR;
    goto cleanup;
  }

  /* Give debuggers a well known point at which xlate_base is known.  */
  NaClGdbHook(nap);

  errcode = NACL_SRPC_RESULT_OK;

 cleanup:

  rval = (*nexe_binary->vtbl->Close)(nexe_binary, &chan->eff.base);
  if (0 != rval) {
    /* Fail the request even though we could go on. */
    errcode = NACL_SRPC_RESULT_NO_MEMORY;
  }
  return errcode;
}

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
/*
 * This RPC is invoked by the plugin as part of the initialization process and
 * provides the NaCl process with a socket address that can later be used for
 * pid to process handle mapping queries. This is required to enable IMC handle
 * passing inside the Chrome sandbox.
 */
static NaClSrpcError NaClInitHandlePassingRpc(
    struct NaClSrpcChannel  *chan,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args) {
  struct NaClApp  *nap = (struct NaClApp *) chan->server_instance_data;
  NaClSrpcImcDescType handle_passing_socket_address = in_args[0]->u.hval;
  DWORD renderer_pid = in_args[1]->u.ival;
  NaClHandle renderer_handle = (NaClHandle)in_args[2]->u.ival;
  UNREFERENCED_PARAMETER(out_args);
  if (!NaClHandlePassLdrCtor(handle_passing_socket_address,
                             renderer_pid,
                             renderer_handle))
    return NACL_SRPC_RESULT_APP_ERROR;
  return NACL_SRPC_RESULT_OK;
}
#endif

static NaClSrpcError NaClSecureChannelSetOriginRpc(
    struct NaClSrpcChannel   *chan,
    struct NaClSrpcArg       **in_args,
    struct NaClSrpcArg       **out_args) {
  struct NaClApp  *nap = (struct NaClApp *) chan->server_instance_data;

  UNREFERENCED_PARAMETER(chan);
  UNREFERENCED_PARAMETER(out_args);

  free(nap->origin);
  nap->origin = strdup(in_args[0]->u.sval);
  NaClLog(4, "NaClSecureChannelSetOriginRpc: origin %s\n", nap->origin);
  return NACL_SRPC_RESULT_OK;
}

static NaClSrpcError NaClSecureChannelStartModuleRpc(
    struct NaClSrpcChannel *chan,
    struct NaClSrpcArg     **in_args,
    struct NaClSrpcArg     **out_args) {
  /*
   * let module start if module is okay; otherwise report error (e.g.,
   * ABI version mismatch).
   */
  struct NaClApp  *nap = (struct NaClApp *) chan->server_instance_data;
  NaClErrorCode   status;

  UNREFERENCED_PARAMETER(in_args);

  NaClLog(4, "NaClSecureChannelStartModuleRpc started\n");
  NaClXMutexLock(&nap->mu);
  while (LOAD_STATUS_UNKNOWN == (status = nap->module_load_status)) {
    NaClCondVarWait(&nap->cv, &nap->mu);
  }
  nap->module_may_start = 1;
  NaClXCondVarBroadcast(&nap->cv);
  NaClXMutexUnlock(&nap->mu);
  out_args[0]->u.ival = status;
  NaClLog(4, "NaClSecureChannelStartModuleRpc finished\n");
  return NACL_SRPC_RESULT_OK;
}

static NaClSrpcError NaClSecureChannelLog(struct NaClSrpcChannel  *chan,
                                          struct NaClSrpcArg      **in_args,
                                          struct NaClSrpcArg      **out_args) {
  int severity = in_args[0]->u.ival;
  char *msg = in_args[1]->u.sval;

  UNREFERENCED_PARAMETER(chan);
  UNREFERENCED_PARAMETER(out_args);

  NaClLog(5, "NaClSecureChannelLog started\n");
  NaClLog(severity, "%s\n", msg);
  NaClLog(5, "NaClSecureChannelLog finished\n");
  return NACL_SRPC_RESULT_OK;
}

void NaClWaitForModuleStartStatusCall(struct NaClApp *nap) {
  NaClLog(4, "NaClWaitForModuleStartStatusCall started\n");
  NaClXMutexLock(&nap->mu);
  while (!nap->module_may_start) {
    NaClXCondVarWait(&nap->cv, &nap->mu);
  }
  NaClXMutexUnlock(&nap->mu);
  NaClLog(4, "NaClWaitForModuleStartStatusCall finished\n");
}

void WINAPI NaClSecureChannelThread(void *state) {
  struct NaClApp                    *nap = (struct NaClApp *) state;

  static struct NaClSrpcHandlerDesc secure_handlers[] = {
    { "hard_shutdown::", NaClSecureChannelShutdownRpc, },
    { "start_module::i", NaClSecureChannelStartModuleRpc, },
    { "set_origin:s:", NaClSecureChannelSetOriginRpc, },
    { "log:is:", NaClSecureChannelLog, },
    { "load_module:h:", NaClLoadModuleRpc, },
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
    { "init_handle_passing:hii:", NaClInitHandlePassingRpc, },
#endif
    /* add additional calls here.  upcall set up?  start module signal? */
    { (char const *) NULL, (NaClSrpcMethod) 0, },
  };

  NaClLog(4, "NaClSecureChannelThread started\n");

  (void) NaClSrpcServerLoop(nap->secure_channel, secure_handlers, nap);
  NaClLog(4, "NaClSecureChannelThread: channel closed, exiting.\n");
  _exit(0);
}

void NaClSecureCommandChannel(struct NaClApp  *nap) {
  struct NaClNrdXferEffector  nnxep;
  int                         status;

  NaClLog(4, "Waiting for secure command channel connect\n");
  (void) NaClNrdXferEffectorCtor(&nnxep, nap->service_port);
  /*
   * this block until the plugin connects
   */
  status = ((*nap->service_port->vtbl->AcceptConn)
            (nap->service_port,
             (struct NaClDescEffector *) &nnxep));
  if (status < 0) {
    NaClLog(LOG_FATAL,
            "NaClSecureCommandChannel: unable to establish channel\n");
  }
  nap->secure_channel = NaClNrdXferEffectorTakeDesc(&nnxep);

  (*nnxep.base.vtbl->Dtor)((struct NaClDescEffector *) &nnxep);
  /*
   * Spawn secure channel thread.
   */
  NaClThreadCtor(&nap->secure_channel_thr,
                 NaClSecureChannelThread,
                 nap,
                 NACL_KERN_STACK_SIZE);
  NaClLog(4, "NaClSecureCommandChannel: thread spawned, continuing\n");
}

void NaClDumpServiceAddressTo(struct NaClApp  *nap,
                              int             desc) {
  NaClLog(4,
          "NaClDumpServiceAddressTo(0x%08"NACL_PRIxPTR", %d)\n",
          (uintptr_t) nap,
          desc);
  if (NULL == nap->service_address) {
    NaClLog(LOG_FATAL,
            "NaClDumpServiceAddressTo: service address not set\n");
    return;
  }
  if (sizeof ((struct NaClDescConnCap *) nap->service_address)->cap.path
      != write(desc,
               ((struct NaClDescConnCap *) nap->service_address)->cap.path,
               sizeof
               ((struct NaClDescConnCap *) nap->service_address)->cap.path)) {
    NaClLog(LOG_FATAL,
            "NaClDumpServiceAddressTo: could not send service address\n");
  }
}

struct NaClFreeState {
  struct NaClApp          *nap;
  struct NaClDescEffector *effp;
  uintptr_t               partial;
  size_t                  pbytes;
};

static void NaClAppFreeWalker(void                  *state,
                              struct NaClVmmapEntry *entry) {
  struct NaClFreeState            *p = (struct NaClFreeState *) state;
  uintptr_t                       sysaddr;
  size_t                          nbytes;

  NaClLog(3,
          ("NaClAppFreeWalker: p->partial = 0x%08"NACL_PRIxPTR","
           " p->pbytes = 0x%08"NACL_PRIxS"\n"),
          p->partial, p->pbytes);
  NaClLog(3,
          (" entry->page_num = 0x%"NACL_PRIxPTR", entry->npages = 0x%"
           NACL_PRIxS","
           " entry->nmop = 0x%08"NACL_PRIxPTR"\n"),
          entry->page_num, entry->npages, (uintptr_t) entry->nmop);
  sysaddr = NaClUserToSysAddrNullOkay(p->nap,
                                      entry->page_num << NACL_PAGESHIFT);
  nbytes = entry->npages << NACL_PAGESHIFT;
  if (NULL == entry->nmop) {
    if (0 != p->pbytes) {
      /* partial exists, accumulate and try to free */
      if (p->partial + p->pbytes != sysaddr) {
        NaClLog(LOG_FATAL,
                ("Partial allocation pages not contiguous?!?"
                 " Partial start 0x%08"NACL_PRIxPTR", length 0x%"NACL_PRIxS";"
                 " next start 0x%08"NACL_PRIxPTR"\n"),
                p->partial, p->pbytes, sysaddr);
      }
      p->pbytes += nbytes;
      if (NaClRoundHostAllocPage(p->pbytes) == p->pbytes) {
        NaCl_page_free((void *) p->partial, p->pbytes);
        p->partial = 0;
        p->pbytes = 0;
      } else {
        NaClLog(3, "Partial accummulated 0x%08"NACL_PRIxPTR", 0x%"
                NACL_PRIxS"\n",
                p->partial, p->pbytes);
      }
    } else {
      /* free if we can; else accumulate */
      if (NaClRoundHostAllocPage(nbytes) == nbytes) {
        NaCl_page_free((void *) sysaddr, nbytes);
      } else {
        p->partial = sysaddr;
        p->pbytes = nbytes;
      }
    }
  } else {
    uintptr_t user_address = entry->page_num << NACL_PAGESHIFT;
    if (NaClRoundAllocPage(user_address) != user_address) {
      NaClLog(LOG_FATAL,
              ("descriptor backed memory does not start"
               " at allocation boundary, addr: 0x%08"NACL_PRIxPTR"\n"),
              user_address);
    }
    if (NaClRoundHostAllocPage(nbytes) != nbytes) {
      NaClLog(LOG_FATAL,
              ("descriptor backed memory size not allocation granularity:"
               " 0x%"NACL_PRIxS"\n"),
              nbytes);
    }
    if (0 != (*entry->nmop->ndp->vtbl->UnmapUnsafe)(entry->nmop->ndp,
                                                    p->effp,
                                                    (void *) sysaddr,
                                                    nbytes)) {
      NaClLog(LOG_FATAL,
              "NaClAppFreeWalker could not unmap memory\n");
    }
  }
}

void NaClAppFreeAllMemory(struct NaClApp  *nap) {
  struct NaClFreeState            state;
  struct NaClDescEffectorCleanup  eff;

  NaClLog(4, "NaClAppFreeAllMemory: invoking TeardownMprotectGuards\n");
  NaClTeardownMprotectGuards(nap);
  NaClLog(4, "NaClAppFreeAllMemory: TeardownMprotectGuards done\n");

  state.nap = nap;
  state.effp = (struct NaClDescEffector *) &eff;
  state.partial = 0;
  state.pbytes = 0;

  NaClDescEffectorCleanupCtor(&eff);
  NaClVmmapVisit(&nap->mem_map, NaClAppFreeWalker, &state);
  (*eff.base.vtbl->Dtor)(&eff.base);
}


#ifdef __GNUC__

/*
 * GDB's canonical overlay managment routine.
 * We need its symbol in the symbol table so don't inline it.
 * TODO(dje): add some explanation for the non-GDB person.
 */

static void __attribute__ ((noinline)) _ovly_debug_event (void) {
  /*
   * The asm volatile is here as instructed by the GCC docs.
   * It's not enough to declare a function noinline.
   * GCC will still look inside the function to see if it's worth calling.
   */
  asm volatile ("");
}

#endif

static void StopForDebuggerInit (const struct NaClApp *state) {
  /* Put xlate_base in a place where gdb can find it.  */
  nacl_global_xlate_base = state->mem_start;

#ifdef __GNUC__
  _ovly_debug_event ();
#endif
}

void NaClGdbHook(struct NaClApp const *nap) {
  StopForDebuggerInit(nap);
}
