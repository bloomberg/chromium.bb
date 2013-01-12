/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client support for thread local storage
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/tls_params.h"

/*
 * Symbols defined by the linker, we copy those sections using them as
 * templates.  These are defined by older hacked-up nacl-binutils linkers,
 * but not by the next-generation (upstream) linkers.  To make this code
 * compatible with both kinds of linker, we use these only as weak
 * references.  With a linker that provides them, they will always be
 * defined.  With another linker, they will always be zero and we rely
 * on such a linker using a layout wherein the phdrs are visible in the
 * address space and the linker gives us __ehdr_start.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[3] */
extern char __tls_template_start __attribute__((weak));
extern char __tls_template_tdata_end __attribute__((weak));
extern char __tls_template_end __attribute__((weak));
/*
 * __tls_template_alignment is defined by PNaCl's ExpandTls LLVM pass
 * but not by linker scripts.
 */
extern uint32_t __tls_template_alignment __attribute__((weak));

/*
 * In a proper implementation, there is no single fixed alignment for the
 * TLS data block.  The linker aligns .tdata as its particular constituents
 * require, and the PT_TLS phdr's p_align field tells the runtime what that
 * requirement is.  But in NaCl we do not have access to our own program's
 * phdrs at runtime in a statically-linked program.  Instead, we just know
 * that the linker has been hacked always to give it 16-byte alignment and
 * we hope that the actual requirement is no larger than that.
 */
#define TLS_PRESUMED_ALIGNMENT   16

/*
 * The next-generation (upstream) linkers define this symbol when
 * the ELF file and program headers are visible in the address space.
 * We can use this to bootstrap finding PT_TLS when it's available.
 * We use a weak reference to be compatible with the old nacl-binutils
 * linkers and layout, where this symbol is not defined and the headers
 * are not visible in the address space.
 */
/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern const Elf32_Ehdr __ehdr_start __attribute__((weak));

/*
 * Collect information about the TLS initializer data here.
 * The first call to get_tls_info() fills in all the data,
 * based either on PT_TLS or on link-time values and presumptions.
 */

struct tls_info {
  const void *tdata_start;  /* Address of .tdata (initializer data) */
  size_t tdata_size;        /* Size of .tdata (initializer data) */
  size_t tbss_size;         /* Size of .tbss (zero-fill space after .tdata) */
  size_t tls_alignment;     /* Alignment required for TLS segment */
};

static const struct tls_info *get_tls_info(void) {
  static struct tls_info cached_tls_info;
  if (cached_tls_info.tls_alignment == 0) {
    /*
     * This is the first call, so we have to figure out the data.
     * See if we have access to our own PT_TLS fields to tell from there.
     */
    const Elf32_Phdr *phdr = NULL;
    int phnum = 0;
    bool phentsize_ok = false;
    if (&__ehdr_start != NULL && __ehdr_start.e_ident[EI_CLASS] == ELFCLASS32) {
      phentsize_ok = __ehdr_start.e_phentsize == sizeof(Elf32_Phdr);
      phnum = __ehdr_start.e_phnum;
      phdr = (const Elf32_Phdr *) ((const char *) &__ehdr_start +
                                   __ehdr_start.e_phoff);
    }
    if (phentsize_ok && phdr != NULL) {
      /*
       * We have phdrs we can see.  Now find our PT_TLS.
       */
      for (int i = 0; i < phnum; ++i) {
        if (phdr[i].p_type == PT_TLS) {
          cached_tls_info.tls_alignment = phdr[i].p_align;
          cached_tls_info.tdata_start = (const void *) phdr[i].p_vaddr;
          cached_tls_info.tdata_size = phdr[i].p_filesz;
          cached_tls_info.tbss_size = phdr[i].p_memsz - phdr[i].p_filesz;
          break;
        }
      }
    }

    if (cached_tls_info.tls_alignment == 0) {
      /*
       * We didn't find anything that way, so we have to assume that
       * we were built with the hacked-up linker that provides symbols
       * and forces the alignment, or with PNaCl's ExpandTls LLVM
       * pass, which defines the alignment properly.
       */
      cached_tls_info.tls_alignment =
          &__tls_template_alignment
          ? __tls_template_alignment : TLS_PRESUMED_ALIGNMENT;
      cached_tls_info.tdata_start = &__tls_template_start;
      cached_tls_info.tdata_size = (&__tls_template_tdata_end -
                                    &__tls_template_start);
      cached_tls_info.tbss_size = (&__tls_template_end -
                                   &__tls_template_tdata_end);
    }
  }
  return &cached_tls_info;
}

static size_t aligned_size(size_t size, size_t alignment) {
  return (size + alignment - 1) & -alignment;
}

static char *aligned_addr(void *start, size_t alignment) {
  return (void *) aligned_size((size_t) start, alignment);
}

static inline void simple_abort(void) {
  *(volatile int *) 0 = 0;  /* Crash. */
}

/*
 * We support x86 and ARM TLS layouts.
 *
 * x86 layout:
 *  * TLS data + BSS
 *  * padding to round TLS data+BSS size upto tls_alignment
 *  --- thread pointer ($tp) points here
 *  * TDB (thread library's data block)
 *
 * ARM layout:
 *  * TDB (thread library's data block)
 *     * note that no padding follows the TDB
 *  --- thread pointer ($tp) points here
 *  * 8-byte header for use by the thread library
 *  * padding to round 8-byte header upto tls_alignment
 *  * TLS data + BSS
 *
 * The offset from the thread pointer to the TLS data is fixed by the
 * linker.
 *
 * The addresses of the thread pointer and TLS data must both be
 * aligned to tls_alignment.  Since combined_area is not necessarily
 * aligned to tls_alignment, padding may be required at the start of
 * both x86 and ARM TLS layouts (not shown above).
 */

static char *tp_from_combined_area(const struct tls_info *info,
                                   void *combined_area, size_t tdb_size) {
  size_t tls_size = info->tdata_size + info->tbss_size;
  ptrdiff_t tdboff = __nacl_tp_tdb_offset(tdb_size);
  if (tdboff < 0) {
    /*
     * The combined area is big enough to hold the TDB and then be aligned
     * up to the $tp alignment requirement.  If the whole area is aligned
     * to the $tp requirement, then aligning the beginning of the area
     * would give us the beginning unchanged, which is not what we need.
     * Instead, align from the putative end of the TDB, to decide where
     * $tp--the true end of the TDB--should actually lie.
     */
    return aligned_addr((char *) combined_area + tdb_size, info->tls_alignment);
  } else {
    /*
     * The linker increases the size of the TLS block up to its alignment
     * requirement, and that total is subtracted from the $tp address to
     * access the TLS area.  To keep that final address properly aligned,
     * we need to align up from the allocated space and then add the
     * aligned size.
     */
    tls_size = aligned_size(tls_size, info->tls_alignment);
    return aligned_addr((char *) combined_area, info->tls_alignment) + tls_size;
  }
}

void *__nacl_tls_initialize_memory(void *combined_area, size_t tdb_size) {
  const struct tls_info *info = get_tls_info();
  size_t tls_size = info->tdata_size + info->tbss_size;
  char *combined_area_end =
      (char *) combined_area + __nacl_tls_combined_size(tdb_size);
  void *tp = tp_from_combined_area(info, combined_area, tdb_size);
  char *start = tp;

  if (__nacl_tp_tls_offset(0) > 0) {
    /*
     * From $tp, we skip the header size and then must round up from
     * there to the required alignment (which is what the linker will
     * will do when calculating TPOFF relocations at link time).  The
     * end result is that the offset from $tp matches the one chosen
     * by the linker exactly and that the final address is aligned to
     * info->tls_alignment (since $tp was already aligned to at least
     * that much).
     */
    start += aligned_size(__nacl_tp_tls_offset(tls_size), info->tls_alignment);
  } else {
    /*
     * We'll subtract the aligned size of the TLS block from $tp, which
     * must itself already be adequately aligned.
     */
    start += __nacl_tp_tls_offset(aligned_size(tls_size, info->tls_alignment));
  }

  /* Sanity check.  (But avoid pulling in assert() here.) */
  if (start + info->tdata_size + info->tbss_size > combined_area_end)
    simple_abort();
  memcpy(start, info->tdata_start, info->tdata_size);
  memset(start + info->tdata_size, 0, info->tbss_size);

  if (__nacl_tp_tdb_offset(tdb_size) == 0) {
    /*
     * On x86 (but not on ARM), the TDB sits directly at $tp and the
     * first word there must hold the $tp pointer itself.
     */
    void *tdb = (char *) tp + __nacl_tp_tdb_offset(tdb_size);
    *(void **) tdb = tdb;
  }

  return tp;
}

size_t __nacl_tls_combined_size(size_t tdb_size) {
  const struct tls_info *info = get_tls_info();
  size_t tls_size = info->tdata_size + info->tbss_size;
  ptrdiff_t tlsoff = __nacl_tp_tls_offset(tls_size);
  size_t combined_size = tls_size + tdb_size;
  /*
   * __nacl_tls_initialize_memory() accepts a non-aligned pointer; it
   * aligns the thread pointer itself.  We have to reserve some extra
   * space to allow this alignment padding to occur.
   */
  combined_size += info->tls_alignment - 1;
  if (tlsoff > 0) {
    /*
     * ARM case: We have to add ARM's 8 byte header, because that is
     * not incorporated into tls_size.  Furthermore, the header is
     * padded out to tls_alignment.
     */
    combined_size += aligned_size(tlsoff, info->tls_alignment);
  }
  return combined_size;
}
