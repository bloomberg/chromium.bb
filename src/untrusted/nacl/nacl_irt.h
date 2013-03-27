/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_IRT_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_IRT_H_

#include <errno.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/irt/irt.h"

extern TYPE_nacl_irt_query __nacl_irt_query;

extern struct nacl_irt_basic __libnacl_irt_basic;
extern struct nacl_irt_fdio __libnacl_irt_fdio;
extern struct nacl_irt_filename __libnacl_irt_filename;
extern struct nacl_irt_memory_v0_2 __libnacl_irt_memory;
extern struct nacl_irt_tls __libnacl_irt_tls;
extern struct nacl_irt_blockhook __libnacl_irt_blockhook;
extern struct nacl_irt_clock __libnacl_irt_clock;
extern struct nacl_irt_dev_getpid __libnacl_irt_dev_getpid;

extern void __libnacl_mandatory_irt_query(const char *interface_ident,
                                          void *table, size_t table_size);
extern void __libnacl_irt_init(Elf32_auxv_t *auxv);

/*
 * NACL_OPTIONAL_FN is used to create libc wrapper functions.
 *
 * iface is the "name" of the IRT interface, where nacl_irt_ ## iface
 * is the struct tag and __libnacl_irt_ ## iface is the name of the
 * irt variable containing the function pointers.
 *
 * iface_name is the string name of the interface (and version) used
 * in __nacl_irt_query.
 *
 * fname is the name of the libc wrapper function to be defined.
 *
 * arglist is the parenthesized, comma separated parameter list for
 * fname in function invocation or K&R style function declaration.
 *
 * args is the parenthesized, comma separated ANSI-style formal
 * parameter list declarator for fname (complete with type
 * information).  The names of the formal parameter identifiers must
 * match those in arglist.
 *
 * This function directly modifies the __libnacl_irt_ ## iface object
 * and depends upon knowledge that nacl_irt_interface (pointed to by
 * __nacl_irt_query) will do a memcpy to update the interface object,
 * and that since memcpy is doing word aligned updates and if two
 * threads doing simultaneous updates to this object is idempotent, we
 * will not run into thread safety issues.  If the interface query
 * fails, we fill the function pointer with the fname ##
 * _not_implemented function.  Note, however, that we do not fill the
 * entire interface table, so that an invocation of a libc wrapper
 * function that makes use of a different interface function pointer
 * will re-do the IRT interface lookup.  This is a limitation of this
 * approach, since we cannot enumerate through all struct members.
 *
 */
#define NACL_OPTIONAL_FN(iface_obj, iface_name, fname, arglist, args) \
  static int fname ## _not_implemented args {                         \
    return ENOSYS;                                                    \
  }                                                                   \
  int fname args {                                                    \
    int error;                                                        \
    if (NULL == iface_obj.fname) {                                    \
      if (__nacl_irt_query(iface_name,                                \
                           &iface_obj,                                \
                           sizeof(iface_obj)) !=                      \
          sizeof(iface_obj)) {                                        \
        iface_obj.fname = fname ## _not_implemented;                  \
      }                                                               \
    }                                                                 \
    error = iface_obj.fname arglist;                                  \
    if (0 != error) {                                                 \
      errno = error;                                                  \
      return -1;                                                      \
    }                                                                 \
    return 0;                                                         \
  }

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_IRT_H_ */
