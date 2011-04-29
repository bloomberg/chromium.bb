/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE  /* This makes glibc's unistd.h declare environ.  */
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

struct nacl_irt_basic __libnacl_irt_basic;
struct nacl_irt_file __libnacl_irt_file;
struct nacl_irt_memory __libnacl_irt_memory;
struct nacl_irt_dyncode __libnacl_irt_dyncode;
struct nacl_irt_thread __libnacl_irt_thread;
struct nacl_irt_mutex __libnacl_irt_mutex;
struct nacl_irt_cond __libnacl_irt_cond;
struct nacl_irt_sem __libnacl_irt_sem;
struct nacl_irt_tls __libnacl_irt_tls;
struct nacl_irt_blockhook __libnacl_irt_blockhook;

TYPE_nacl_irt_query __nacl_irt_query;

/*
 * TODO(mcgrathr): This extremely stupid function should not exist.
 * If the startup calling sequence were sane, this would be done
 * someplace that has the initial pointer locally rather than stealing
 * it from environ.
 * See http://code.google.com/p/nativeclient/issues/detail?id=651
 */
static Elf32_auxv_t *find_auxv(void) {
  /*
   * This presumes environ has its startup-time value on the stack.
   */
  char **ep = environ;
  while (*ep != NULL)
    ++ep;
  return (void *) (ep + 1);
}

/*
 * Scan the auxv for AT_SYSINFO, which is the pointer to the IRT query function.
 * Stash that for later use.
 */
static void grok_auxv(const Elf32_auxv_t *auxv) {
  const Elf32_auxv_t *av;
  for (av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO) {
      __nacl_irt_query = (TYPE_nacl_irt_query) av->a_un.a_val;
    }
  }
}

#define DO_QUERY(ident, name)                                                 \
  do_irt_query(ident, &__libnacl_irt_##name,                                  \
               sizeof(__libnacl_irt_##name), &nacl_irt_##name)

static void do_irt_query(const char *interface_ident,
                         void *buffer, size_t table_size,
                         const void *fallback) {
  if (NULL == __nacl_irt_query ||
      __nacl_irt_query(interface_ident, buffer, table_size) != table_size) {
    memcpy(buffer, fallback, table_size);
  }
}

/*
 * Initialize all our IRT function tables using the query function.
 * The query function's address is passed via AT_SYSINFO in auxv.
 */
void __libnacl_irt_init(void) {
  grok_auxv(find_auxv());

  DO_QUERY(NACL_IRT_BASIC_v0_1, basic);
  DO_QUERY(NACL_IRT_FILE_v0_1, file);
  DO_QUERY(NACL_IRT_MEMORY_v0_1, memory);
  DO_QUERY(NACL_IRT_DYNCODE_v0_1, dyncode);
  DO_QUERY(NACL_IRT_THREAD_v0_1, thread);
  DO_QUERY(NACL_IRT_MUTEX_v0_1, mutex);
  DO_QUERY(NACL_IRT_COND_v0_1, cond);
  DO_QUERY(NACL_IRT_SEM_v0_1, sem);
  DO_QUERY(NACL_IRT_TLS_v0_1, tls);
  DO_QUERY(NACL_IRT_BLOCKHOOK_v0_1, blockhook);
}
