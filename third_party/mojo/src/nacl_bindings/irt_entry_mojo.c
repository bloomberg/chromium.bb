// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/platform/nacl/mojo_irt.h"
#include "native_client/src/public/irt_core.h"

static size_t irt_query(const char* interface_ident,
                        void* table, size_t tablesize) {
  // TODO(teravest): Add query for the Mojo IRT interface, when introduced.
  size_t rc = mojo_irt_query(interface_ident, table, tablesize);
  if (rc != 0)
    return rc;

  return nacl_irt_query_core(interface_ident, table, tablesize);
}

void nacl_irt_start(uint32_t* info) {
  nacl_irt_init(info);
  nacl_irt_enter_user_code(info, irt_query);
}
