/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_IRT_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_IRT_H_

#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/irt/irt.h"

extern TYPE_nacl_irt_query __nacl_irt_query;

extern struct nacl_irt_basic __libnacl_irt_basic;
extern struct nacl_irt_fdio __libnacl_irt_fdio;
extern struct nacl_irt_filename __libnacl_irt_filename;
extern struct nacl_irt_memory __libnacl_irt_memory;
extern struct nacl_irt_tls __libnacl_irt_tls;
extern struct nacl_irt_blockhook __libnacl_irt_blockhook;

extern void __libnacl_mandatory_irt_query(const char *interface_ident,
                                          void *table, size_t table_size);
extern void __libnacl_irt_init(Elf32_auxv_t *auxv);

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_IRT_H_ */
