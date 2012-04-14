/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/trusted/service_runtime/nacl_copy.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

int NaClCopyInFromUser(struct NaClApp *nap,
                       void           *dst_sys_ptr,
                       uintptr_t      src_usr_addr,
                       size_t         num_bytes) {
  uintptr_t src_sys_addr;

  src_sys_addr = NaClUserToSysAddrRange(nap, src_usr_addr, num_bytes);
  if (kNaClBadAddress == src_sys_addr) {
    return 0;
  }
  /*
   * TODO(bsy): grab address space lock -- do not need to do it before
   * NaClUserToSysAddrRange check, since that does not verify whether
   * an address is mapped or not, just that it's within 1 << nap->addr_bits.
   */
  memcpy((void *) dst_sys_ptr, (void *) src_sys_addr, num_bytes);
  /* TODO(bsy): drop address space lock */
  return 1;
}

int NaClCopyInFromUserZStr(struct NaClApp *nap,
                           char           *dst_buffer,
                           size_t         dst_buffer_bytes,
                           uintptr_t      src_usr_addr) {
  uintptr_t src_sys_addr;

  CHECK(dst_buffer_bytes > 0);
  src_sys_addr = NaClUserToSysAddr(nap, src_usr_addr);
  if (kNaClBadAddress == src_sys_addr) {
    dst_buffer[0] = '\0';
    return 0;
  }
  /*
   * TODO(bsy): grab address space lock -- do not need to do it before
   * NaClUserToSysAddrRange check, since that does not verify whether
   * an address is mapped or not, just that it's within 1 << nap->addr_bits.
   */
  strncpy(dst_buffer, (char *) src_sys_addr, dst_buffer_bytes);
  /* TODO(bsy): drop address space lock */

  /* POSIX strncpy pads with NUL characters */
  if (dst_buffer[dst_buffer_bytes - 1] != '\0') {
    dst_buffer[dst_buffer_bytes - 1] = '\0';
    return 0;
  }
  return 1;
}


int NaClCopyOutToUser(struct NaClApp  *nap,
                      uintptr_t       dst_usr_addr,
                      void            *src_sys_ptr,
                      size_t          num_bytes) {
  uintptr_t dst_sys_addr;

  dst_sys_addr = NaClUserToSysAddrRange(nap, dst_usr_addr, num_bytes);
  if (kNaClBadAddress == dst_sys_addr) {
    return 0;
  }
  /*
   * TODO(bsy): grab address space lock -- do not need to do it before
   * NaClUserToSysAddrRange check, since that does not verify whether
   * an address is mapped or not, just that it's within 1 << nap->addr_bits.
   */
  memcpy((void *) dst_sys_addr, src_sys_ptr, num_bytes);
  /* TODO(bsy): drop address space lock */

  return 1;
}
