/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_extension.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

struct nacl_irt_ext_struct {
  const char *interface_ident;
  void *table;
  size_t tablesize;
};

static const struct nacl_irt_ext_struct nacl_irt_ext_structs[] = {
  {
    .interface_ident = NACL_IRT_FDIO_v0_1,
    .table = &__libnacl_irt_fdio,
    .tablesize = sizeof(__libnacl_irt_fdio),
  },
  {
    .interface_ident = NACL_IRT_DEV_FDIO_v0_3,
    .table = &__libnacl_irt_dev_fdio,
    .tablesize = sizeof(__libnacl_irt_dev_fdio),
  }, {
    .interface_ident = NACL_IRT_DEV_FILENAME_v0_3,
    .table = &__libnacl_irt_dev_filename,
    .tablesize = sizeof(__libnacl_irt_dev_filename),
  }, {
    .interface_ident = NACL_IRT_MEMORY_v0_3,
    .table = &__libnacl_irt_memory,
    .tablesize = sizeof(struct nacl_irt_memory),
  },
};

size_t nacl_interface_ext_supply(const char *interface_ident,
                               const void *table, size_t tablesize) {
  for (int i = 0; i < NACL_ARRAY_SIZE(nacl_irt_ext_structs); i++) {
    if (nacl_irt_ext_structs[i].tablesize == tablesize &&
        strcmp(nacl_irt_ext_structs[i].interface_ident, interface_ident) == 0) {
      memcpy(nacl_irt_ext_structs[i].table, table, tablesize);
      return tablesize;
    }
  }

  return 0;
}
