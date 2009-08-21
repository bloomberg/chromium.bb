/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*******************************************************************************
 *
 * DO NOT INCLUDE EXCEPT FROM sel_ldr.h and sel_ldr-inl.c
 *
 * THERE CANNOT BE ANY MULTIPLE INCLUSION GUARDS IN ORDER FOR
 * sel_ldr-inl.c TO WORK.
 *
 ******************************************************************************/

/*
 * Routines to translate addresses between user and "system" or
 * service runtime addresses.  the *Addr* versions will return
 * kNaClBadAddress if the user address is outside of the user address
 * space, e.g., if the input addresses for *UserToSys* is outside of
 * (1<<nap->addr_bits), and correspondingly for *SysToUser* if the
 * input system address does not correspond to a user address.
 * Generally, the *Addr* versions are used when the addresses come
 * from untrusted usre code, and kNaClBadAddress would translate to an
 * EINVAL return from a syscall.  The *Range code ensures that the
 * entire address range is in the user address space.
 *
 * Note that just because an address is within the address space, it
 * doesn't mean that it is safe to acceess the memory: the page may be
 * protected against access.
 *
 * The non-*Addr* versions abort the program rather than return an
 * error indication.
 */

/*
 * address translation routines.  after a NaClApp is started, the
 * member variables accessed by these routines are read-only, so no
 * locking is needed to use these functions, as long as the NaClApp
 * structure doesn't get destructed/deallocated.
 *
 * the first is used internally when a NULL pointer is okay, typically
 * for address manipulation.
 *
 * the next two are for syscalls to do address translation, e.g., for
 * system calls; -1 indicates an error, so the syscall can return
 * EINVAL or EFAULT or whatever is appropriate.
 *
 * the latter two interfaces are for use everywhere else in the loader
 * / service runtime and will log a fatal error and abort the process
 * when an error is detected.  (0 is not a good error indicator, since
 * 0 is a valid user address.)
 */

static INLINE uintptr_t NaClUserToSysAddrNullOkay(struct NaClApp  *nap,
                                                  uintptr_t       uaddr) {
  if (uaddr >= (1U << nap->addr_bits)) {
    return kNaClBadAddress;
  }
  return uaddr + nap->xlate_base;
}

static INLINE uintptr_t NaClUserToSysAddr(struct NaClApp  *nap,
                                          uintptr_t       uaddr) {
  if (0 == uaddr || uaddr >= (1U << nap->addr_bits)) {
    return kNaClBadAddress;
  }
  return uaddr + nap->xlate_base;
}

static INLINE uintptr_t NaClSysToUserAddr(struct NaClApp  *nap,
                                          uintptr_t       sysaddr) {
  if (sysaddr < nap->mem_start ||
      sysaddr >= nap->mem_start + (1U << nap->addr_bits)) {
    return kNaClBadAddress;
  }
  return sysaddr - nap->xlate_base;
}

static INLINE uintptr_t NaClUserToSysAddrRange(struct NaClApp  *nap,
                                               uintptr_t       uaddr,
                                               size_t          count) {
  uintptr_t end_addr;

  if (0 == uaddr) {
    return kNaClBadAddress;
  }
  end_addr = uaddr + count;
  if (end_addr < uaddr) {
    /* unsigned wraparound */
    return kNaClBadAddress;
  }
  if (end_addr >= (1U << nap->addr_bits)) {
    return kNaClBadAddress;
  }
  return uaddr + nap->xlate_base;
}

static INLINE uintptr_t NaClUserToSys(struct NaClApp  *nap,
                                      uintptr_t       uaddr) {
  if (0 == uaddr || uaddr >= (1U << nap->addr_bits)) {
    NaClLog(LOG_FATAL,
            "NaClUserToSys: uaddr 0x%08"PRIxPTR", addr space %u bits\n",
            uaddr, nap->addr_bits);
  }
  return uaddr + nap->xlate_base;
}

static INLINE uintptr_t NaClSysToUser(struct NaClApp  *nap,
                                      uintptr_t       sysaddr) {
  if (sysaddr < nap->mem_start ||
      sysaddr >= nap->mem_start + (1U << nap->addr_bits)) {
    NaClLog(LOG_FATAL,
            "NaclSysToUser: sysaddr 0x%08"PRIxPTR", mem_start 0x%08"PRIxPTR","
            " addr space %d bits\n",
            sysaddr, nap->mem_start, nap->addr_bits);
  }
  return sysaddr - nap->xlate_base;
}
