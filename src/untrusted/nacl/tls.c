/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client support for thread local storage
 */

#include <stdint.h>
#include <string.h>

#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/tls_params.h"

/*
 * Symbols defined by the linker, we copy those sections using them
 * as templates.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[3] */
extern char __tls_template_start;
extern char __tls_template_tdata_end;
extern char __tls_template_end;

/* Locate the TLS sections using symbols created by the linker. */
#define TLS_TDATA_START (&__tls_template_start)
#define TLS_TDATA_SIZE  (&__tls_template_tdata_end - TLS_TDATA_START)
#define TLS_TBSS_SIZE   (&__tls_template_end - &__tls_template_tdata_end)

/*
 * In a proper implementation, there is no single fixed alignment for the
 * TLS data block.  The linker aligns .tdata as its particular constituents
 * require, and the PT_TLS phdr's p_align field tells the runtime what that
 * requirement is.  But in NaCl we do not have access to our own program's
 * phdrs at runtime in a statically-linked program.  Instead, we just know
 * that the linker has been hacked always to give it 16-byte alignment and
 * we hope that the actual requirement is no larger than that.
 */
#define TLS_ALIGNMENT   16

static size_t aligned_size(size_t size, size_t alignment) {
  return (size + alignment - 1) & -alignment;
}

static char *aligned_addr(void *start, size_t alignment) {
  return (void *) aligned_size((size_t) start, alignment);
}

static char *tp_from_combined_area(void *combined_area, size_t tdb_size) {
  size_t tls_size = TLS_TDATA_SIZE + TLS_TBSS_SIZE;
  ptrdiff_t tdboff = __nacl_tp_tdb_offset(tdb_size);
  if (tdboff < 0) {
    /*
     * ARM case:
     *  +-----------+--------+----------------+
     *  |   TDB     | header | TLS data, bss  |
     *  +-----------+--------+----------------+
     *              ^        ^
     *              |        |
     *              |        +--- $tp+8 points here
     *              |
     *              +--- $tp points here
     *
     * The combined area is big enough to hold the TDB and then be aligned
     * up to the $tp alignment requirement.  If the whole area is aligned
     * to the $tp requirement, then aligning the beginning of the area
     * would give us the beginning unchanged, which is not what we need.
     * Instead, align from the putative end of the TDB, to decide where
     * $tp--the true end of the TDB--should actually lie.
     */
    size_t align = aligned_size(TLS_ALIGNMENT, __nacl_tp_alignment());
    return aligned_addr((char *) combined_area + tdb_size, align);
  } else {
    /*
     * x86 case:
     *  +-----------------+------+
     *  |  TLS data, bss  | TDB  |
     *  +-----------------+------+
     *                    ^
     *                    |
     *                    +--- $tp points here
     *                    |
     *                    +--- first word's value is $tp address
     */
    size_t align = aligned_size(TLS_ALIGNMENT, __nacl_tp_alignment());
    return aligned_addr((char *) combined_area + tls_size, align);
  }
}

static void *tls_from_tp(void *tp, size_t tls_size) {
  return aligned_addr((char *) tp + __nacl_tp_tls_offset(tls_size),
                      TLS_ALIGNMENT);
}

void *__nacl_tls_data_bss_initialize_from_template(void *combined_area,
                                                   size_t tdb_size) {
  size_t tls_size = TLS_TDATA_SIZE + TLS_TBSS_SIZE;
  void *tp = tp_from_combined_area(combined_area, tdb_size);
  char *start = tls_from_tp(tp, tls_size);
  memcpy(start, TLS_TDATA_START, TLS_TDATA_SIZE);
  memset(start + TLS_TDATA_SIZE, 0, TLS_TBSS_SIZE);
  return tp;
}

size_t __nacl_tls_combined_size(size_t tdb_size) {
  size_t tls_size = TLS_TDATA_SIZE + TLS_TBSS_SIZE;
  ptrdiff_t tlsoff = __nacl_tp_tls_offset(tls_size);
  if (tlsoff > 0) {
    /*
     * ARM case:
     *
     *  +-----------+--------+----------------+
     *  |   TDB     | header | TLS data, bss  |
     *  +-----------+--------+----------------+
     *              ^        ^
     *              |        |
     *              |        +--- $tp+8 points here
     *              |
     *              +--- $tp points here
     *
     * The TDB alignment doesn't matter too much.
     */
    return (tdb_size + __nacl_tp_alignment() - 1 +
            tlsoff + tls_size + TLS_ALIGNMENT - 1);
  } else {
    /*
     * x86 case:
     *  +-----------------+------+
     *  |  TLS data, bss  | TDB  |
     *  +-----------------+------+
     *                    ^
     *                    |
     *                    +--- $tp points here
     *                    |
     *                    +--- first word's value is $tp address
     *
     * The TDB alignment doesn't matter too much.
     */
    size_t align = aligned_size(TLS_ALIGNMENT, __nacl_tp_alignment());
    return align - 1 + tls_size + tdb_size;
  }
}

/*
 * Final setup of the memory allocated for TLS space.
 */
void *__nacl_tls_initialize_memory(void *combined_area, size_t tdb_size) {
  void *tp = __nacl_tls_data_bss_initialize_from_template(combined_area,
                                                          tdb_size);

  if (__nacl_tp_tdb_offset(tdb_size) == 0) {
    /*
     * The TDB sits directly at $tp and the first word there must
     * hold the $tp pointer itself.
     */
    void *tdb = (char *) tp + __nacl_tp_tdb_offset(tdb_size);
    *(void **) tdb = tdb;
  }

  return tp;
}
