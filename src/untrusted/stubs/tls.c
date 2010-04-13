/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Native Client support for TLS, platform-specific components.
 * These routines are documented in the tls.h header included below.
 */

#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

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

#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
#define TLS_ALIGNMENT (1 << 12)
#else
#define TLS_ALIGNMENT 32
#endif

/* Rounds up |offset| to next multiple of |alignment|, a power of 2. */
static uint32_t align(uint32_t offset, uint32_t alignment) {
  return (offset + alignment - 1) & ~(alignment - 1);
}

static char *__nacl_tls_align(void* combined_area) {
  return (char *) align((uint32_t) combined_area, TLS_ALIGNMENT);
}

/* See src/untrusted/nacl/tls.h */
size_t __nacl_tls_combined_size(size_t tdb_size) {
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  /* On ARM, the TDB is actually the first symbol in the TLS, so don't
   * count it separately. */
  tdb_size = 0;
#endif
  return TLS_ALIGNMENT - 1 + TLS_TDATA_SIZE + TLS_TBSS_SIZE + tdb_size;
}

/* See src/untrusted/nacl/tls.h */
void* __nacl_tls_tdb_start(void* combined_area) {
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  size_t tdb_offset_in_tls = 0; /* TDB is the first symbol in the TLS. */
#else
  size_t tdb_offset_in_tls = TLS_TDATA_SIZE + TLS_TBSS_SIZE; /* TDB after TLS */
#endif
  return __nacl_tls_align(combined_area) + tdb_offset_in_tls;
}

/* See src/untrusted/nacl/tls.h */
void __nacl_tls_data_bss_initialize_from_template(void* combined_area) {
  char* start = __nacl_tls_align(combined_area);
  memcpy(start, TLS_TDATA_START, TLS_TDATA_SIZE);
  memset(start + TLS_TDATA_SIZE, 0, TLS_TBSS_SIZE);
}

#ifdef __x86_64__
/* Compiler intrinsic. */
void *__tls_get_addr(int offset) {
  return ((char*) NACL_SYSCALL(tls_get)()) + offset;
}
#endif

/* See src/untrusted/nacl/tls.h */
size_t __nacl_return_address_size() {
  /* We can't use sizeof(intptr_t) without relying stdint.h. */
#ifdef __x86_64__
  return 8;
#else /* __arm__ or __i386__ */
  return 4;
#endif
}
