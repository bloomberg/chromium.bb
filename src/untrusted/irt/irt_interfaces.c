/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

struct nacl_interface_table {
  const char *name;
  const void *table;
  size_t size;
  int (*filter)(void);
};

static int file_access_filter(void) {
  static int nacl_file_access_enabled = -1;
  if (NACL_UNLIKELY(-1 == nacl_file_access_enabled)) {
    NACL_SYSCALL(sysconf)(NACL_ABI__SC_NACL_FILE_ACCESS_ENABLED,
                          &nacl_file_access_enabled);
  }
  return nacl_file_access_enabled;
}

static int list_mappings_filter(void) {
  static int nacl_list_mappings_enabled = -1;
  if (NACL_UNLIKELY(-1 == nacl_list_mappings_enabled)) {
    NACL_SYSCALL(sysconf)(NACL_ABI__SC_NACL_LIST_MAPPINGS_ENABLED,
                          &nacl_list_mappings_enabled);
  }
  return nacl_list_mappings_enabled;
}

static const struct nacl_interface_table irt_interfaces[] = {
  { NACL_IRT_BASIC_v0_1, &nacl_irt_basic, sizeof(nacl_irt_basic), NULL },
  { NACL_IRT_FDIO_v0_1, &nacl_irt_fdio, sizeof(nacl_irt_fdio), NULL },
  { NACL_IRT_DEV_FDIO_v0_1, &nacl_irt_fdio, sizeof(nacl_irt_fdio), NULL },
  { NACL_IRT_FILENAME_v0_1, &nacl_irt_filename, sizeof(nacl_irt_filename),
    NULL },
  { NACL_IRT_DEV_FILENAME_v0_2, &nacl_irt_dev_filename,
    sizeof(nacl_irt_dev_filename), file_access_filter },
  { NACL_IRT_MEMORY_v0_1, &nacl_irt_memory_v0_1, sizeof(nacl_irt_memory_v0_1),
    NULL },
  { NACL_IRT_MEMORY_v0_2, &nacl_irt_memory_v0_2, sizeof(nacl_irt_memory_v0_2),
    NULL },
  { NACL_IRT_MEMORY_v0_3, &nacl_irt_memory, sizeof(nacl_irt_memory), NULL },
  { NACL_IRT_DYNCODE_v0_1, &nacl_irt_dyncode, sizeof(nacl_irt_dyncode), NULL },
  { NACL_IRT_THREAD_v0_1, &nacl_irt_thread, sizeof(nacl_irt_thread), NULL },
  { NACL_IRT_FUTEX_v0_1, &nacl_irt_futex, sizeof(nacl_irt_futex), NULL },
  { NACL_IRT_MUTEX_v0_1, &nacl_irt_mutex, sizeof(nacl_irt_mutex), NULL },
  { NACL_IRT_COND_v0_1, &nacl_irt_cond, sizeof(nacl_irt_cond), NULL },
  { NACL_IRT_SEM_v0_1, &nacl_irt_sem, sizeof(nacl_irt_sem), NULL },
  { NACL_IRT_TLS_v0_1, &nacl_irt_tls, sizeof(nacl_irt_tls), NULL },
  { NACL_IRT_BLOCKHOOK_v0_1, &nacl_irt_blockhook, sizeof(nacl_irt_blockhook),
    NULL },
  { NACL_IRT_RESOURCE_OPEN_v0_1, &nacl_irt_resource_open,
    sizeof(nacl_irt_resource_open), NULL },
#ifdef IRT_PPAPI
  { NACL_IRT_PPAPIHOOK_v0_1, &nacl_irt_ppapihook, sizeof(nacl_irt_ppapihook),
    NULL },
#endif
  { NACL_IRT_RANDOM_v0_1, &nacl_irt_random, sizeof(nacl_irt_random), NULL },
  { NACL_IRT_CLOCK_v0_1, &nacl_irt_clock, sizeof(nacl_irt_clock), NULL },
  { NACL_IRT_DEV_GETPID_v0_1, &nacl_irt_dev_getpid,
    sizeof(nacl_irt_dev_getpid), NULL },
  { NACL_IRT_EXCEPTION_HANDLING_v0_1, &nacl_irt_exception_handling,
    sizeof(nacl_irt_exception_handling), NULL },
  { NACL_IRT_DEV_LIST_MAPPINGS_v0_1, &nacl_irt_dev_list_mappings,
    sizeof(nacl_irt_dev_list_mappings), list_mappings_filter },
};

size_t nacl_irt_interface(const char *interface_ident,
                          void *table, size_t tablesize) {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(irt_interfaces); ++i) {
    if (0 == strcmp(interface_ident, irt_interfaces[i].name)) {
      if (NULL == irt_interfaces[i].filter || irt_interfaces[i].filter()) {
        const size_t size = irt_interfaces[i].size;
        if (size <= tablesize) {
          memcpy(table, irt_interfaces[i].table, size);
          return size;
        }
      }
      break;
    }
  }
  return 0;
}
