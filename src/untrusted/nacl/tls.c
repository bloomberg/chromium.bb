/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client support for thread local storage
 */

#include <stdint.h>

#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"

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


/* NOTE: Avoid dependence on libc.
   PLEASE DO NOT REPLACE THESE.
   Otherwise you get a circular dependency and typically the
   tests/hello_world/exit.c will have link problems.
   (we do link this code in a the very end, so there is no chance
   to resolve memcpy, etc. unless they were already linked in.)
*/

static void my_memcpy(void* dest, const void* src, size_t size) {
  size_t i;
  for (i = 0; i < size; ++i) {
    *((char*) dest + i) = *((char*) src + i);
  }
}


static void my_memset(void* s, int c, size_t size) {
  size_t i;
  for (i = 0; i < size; ++i) {
   *((char*) s + i) = (char) c;
  }
}


static char* align(uint32_t offset, uint32_t alignment) {
  return (char*) ((offset + alignment - 1) & ~(alignment - 1));
}


void* __nacl_tls_tdb_start(void* combined_area) {
  return align((uint32_t) combined_area, __nacl_tls_aligment()) +
    __nacl_tdb_offset_in_tls(TLS_TDATA_SIZE + TLS_TBSS_SIZE);
}

/* See tls.h. */
void __nacl_tls_data_bss_initialize_from_template(void* combined_area) {
  char* start = (char *) align((uint32_t) combined_area,
                               __nacl_tls_aligment());
  my_memcpy(start, TLS_TDATA_START, TLS_TDATA_SIZE);
  my_memset(start + TLS_TDATA_SIZE, 0, TLS_TBSS_SIZE);
}


/* See tls.h. */
size_t __nacl_tls_combined_size(size_t tdb_size) {
  return __nacl_tls_aligment() - 1 +
    TLS_TDATA_SIZE +
    TLS_TBSS_SIZE +
    __nacl_tdb_effective_payload_size(tdb_size);
}

/*
 * Final setup of the memory allocated for TLS space.
 */
void *__nacl_tls_initialize_memory(void *combined_area) {
  __nacl_tls_data_bss_initialize_from_template(combined_area);

  /* Patch the first word of the TDB to contain the "base" of the TLS area. */
  void *tdb = __nacl_tls_tdb_start(combined_area);
  *(void**) tdb = tdb;

  return tdb;
}
