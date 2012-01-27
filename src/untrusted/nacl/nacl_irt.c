/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <unistd.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

struct nacl_irt_basic __libnacl_irt_basic;
struct nacl_irt_fdio __libnacl_irt_fdio;
struct nacl_irt_filename __libnacl_irt_filename;
struct nacl_irt_memory __libnacl_irt_memory;
struct nacl_irt_dyncode __libnacl_irt_dyncode;
struct nacl_irt_tls __libnacl_irt_tls;
struct nacl_irt_blockhook __libnacl_irt_blockhook;
struct nacl_irt_clock __libnacl_irt_clock;

TYPE_nacl_irt_query __nacl_irt_query;

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

static void stderr_message(const char *message) {
  write(2, message, strlen(message));
}

void __libnacl_mandatory_irt_query(const char *interface,
                                   void *table, size_t table_size) {
  if (NULL == __nacl_irt_query) {
    stderr_message("No IRT interface query routine!\n");
    _exit(-1);
  }
  if (__nacl_irt_query(interface, table, table_size) != table_size) {
    stderr_message("IRT interface query failed for essential interface \"");
    stderr_message(interface);
    stderr_message("\"!\n");
    _exit(-1);
  }
}

#define DO_QUERY(ident, name)                                   \
  __libnacl_mandatory_irt_query(ident, &__libnacl_irt_##name,   \
                                sizeof(__libnacl_irt_##name))

/*
 * Initialize all our IRT function tables using the query function.
 * The query function's address is passed via AT_SYSINFO in auxv.
 */
void __libnacl_irt_init(Elf32_auxv_t *auxv) {
  grok_auxv(auxv);

  DO_QUERY(NACL_IRT_BASIC_v0_1, basic);
  DO_QUERY(NACL_IRT_FDIO_v0_1, fdio);
  DO_QUERY(NACL_IRT_FILENAME_v0_1, filename);
  DO_QUERY(NACL_IRT_MEMORY_v0_1, memory);
  DO_QUERY(NACL_IRT_DYNCODE_v0_1, dyncode);
  DO_QUERY(NACL_IRT_TLS_v0_1, tls);
  DO_QUERY(NACL_IRT_BLOCKHOOK_v0_1, blockhook);
  DO_QUERY(NACL_IRT_CLOCK_v0_1, clock);
}
