/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client support for thread local storage.
 *
 * This is a highly experimental hack which will work with glibc's ld.so.
 * Note: it probably wont work for ARM as is but we have no way
 * of testing this at this point. As we are lacking a dynamic loader
 * for that platform.
 */

#include <stdint.h>

#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/tls_params.h"

/*
 * These are part of ld.so's API and use a non-standard calling convention
 */
#define REGPARM(n) __attribute__((regparm(n)))

extern REGPARM(2) void _dl_get_tls_static_info(int *static_tls_size,
                                               int *static_tls_align);
extern void *_dl_allocate_tls (void *mem);

/*
 * In case there is some mismatch between the combined areas of
 * glibc's and our pthread/tls code we add some safety margin on both ends
 * of the combined area.
 * This is probably not needed, especially since we believe that
 * the part of the (glibc) tdb which is accessed by ld.so is smaller than
 * our tdb.
 * On the other hand, we have not tried this we a big code bases
 * yet and if there were problems without the safty paddding
 * those would be really hard to track down. So it seems prudent
 * to leave them in for a while.
 *
 * TODO(robertm): set safety paddding to zero after we have tried
 *                this with big apps
 */

#define SAFETY_PADDING 256

static size_t aligned_size(size_t size, size_t alignment) {
   return (size + alignment - 1) & -alignment;
}

static char *aligned_addr(void *start, size_t alignment) {
  return (void *) aligned_size((size_t) start, alignment);
}

/*
 * this is ugly, but we need a way to communicate these from
 *  _dl_get_tls_static_info() to
 * __nacl_tls_data_bss_initialize_from_template()
 */
static int tls_size;
static int tls_align;

size_t __nacl_tls_combined_size(size_t tdb_size) {
  _dl_get_tls_static_info (&tls_size, &tls_align);
  /*
   * Disregarding the SAFETY_PADDING we need enough space
   * to align the tdb which is sort of in the middle of combined area
   * hence the extra "tls_size - 1"
   * TODO(robertm): this may need to be adapted for ARM
   */
  return tls_size + tdb_size + (tls_size - 1) + 2 * SAFETY_PADDING;
}

void *__nacl_tls_data_bss_initialize_from_template(void *combined_area,
                                                   size_t tdb_size) {
  if (__nacl_tp_tdb_offset(tdb_size) != 0) {
    /*
     * This needs more work for ARM.
     * For now abort via null pointer dereference.
     */
    while (1) *(volatile int *) 0;
  } else {
    void *tdb = aligned_addr(((char *) combined_area) + tls_size +
                             SAFETY_PADDING , tls_align);
    return _dl_allocate_tls(tdb);
  }
}
