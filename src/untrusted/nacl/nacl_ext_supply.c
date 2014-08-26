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

static size_t ext_struct_memcpy_init(void *dest_table, const void *src_table,
                                     size_t tablesize) {
  memcpy(dest_table, src_table, tablesize);
  return tablesize;
}

struct nacl_irt_ext_struct {
  const char *interface_ident;
  void *table;
  size_t tablesize;
};

static const struct nacl_irt_ext_struct nacl_irt_ext_structs[] = {
  {
    .interface_ident = NACL_IRT_DEV_FDIO_v0_3,
    .table = &__libnacl_irt_dev_fdio,
    .tablesize = sizeof(__libnacl_irt_dev_fdio)
  }, {
    .interface_ident = NACL_IRT_DEV_FDIO_v0_2,
    .table = &__libnacl_irt_dev_fdio,
    .tablesize = sizeof(struct nacl_irt_dev_fdio_v0_2)
  }, {
    .interface_ident = NACL_IRT_FDIO_v0_1,
    .table = &__libnacl_irt_fdio,
    .tablesize = sizeof(__libnacl_irt_fdio)
  }, {
    .interface_ident = NACL_IRT_DEV_FILENAME_v0_3,
    .table = &__libnacl_irt_dev_filename,
    .tablesize = sizeof(__libnacl_irt_dev_filename)
  }, {
    .interface_ident = NACL_IRT_DEV_FILENAME_v0_2,
    .table = &__libnacl_irt_dev_filename,
    .tablesize = sizeof(struct nacl_irt_dev_filename_v0_2)
  }, {
    .interface_ident = NACL_IRT_FILENAME_v0_1,
    .table = &__libnacl_irt_dev_filename,
    .tablesize = sizeof(struct nacl_irt_filename)
  },
};

size_t nacl_interface_ext_supply(const char *interface_ident,
                               const void *table, size_t tablesize) {
  for (int i = 0; i < NACL_ARRAY_SIZE(nacl_irt_ext_structs); i++) {
    if (nacl_irt_ext_structs[i].tablesize == tablesize &&
        strcmp(nacl_irt_ext_structs[i].interface_ident, interface_ident) == 0) {
      return ext_struct_memcpy_init(nacl_irt_ext_structs[i].table,
                                    table,
                                    tablesize);
    }
  }

  return 0;
}
