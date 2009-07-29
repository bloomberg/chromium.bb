/*
 * Copyright 2008, Google Inc.
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

/*
 * NaCl local descriptor table (LDT) managment
 */
#ifndef SERVICE_RUNTIME_NACL_LDT_H__
#define SERVICE_RUNTIME_NACL_LDT_H__ 1

#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

/* TODO(petr): this should go to linux/x86 */
#if NACL_LINUX
/*
 * The modify_ldt system call is used to get and set the local descriptor
 * table.
 */
extern int modify_ldt(int func, void* ptr, unsigned long bytecount);
#endif

/*
 * Module initialization and finalization.
 */
extern int NaClLdtInit();
extern void NaClLdtFini();

/*
 * NaClLdtAllocateSelector creates an entry installed in the local descriptor
 * table. If successfully installed, it returns a non-zero selector.  If it
 * fails to install the entry, it returns zero.
 */
typedef enum {
  NACL_LDT_DESCRIPTOR_DATA,
  NACL_LDT_DESCRIPTOR_CODE
} NaClLdtDescriptorType;

uint16_t NaClLdtAllocatePageSelector(NaClLdtDescriptorType type,
                                     int read_exec_only,
                                     void* base_addr,
                                     uint32_t size_in_pages);

uint16_t NaClLdtAllocateByteSelector(NaClLdtDescriptorType type,
                                     int read_exec_only,
                                     void* base_addr,
                                     uint32_t size_in_bytes);

uint16_t NaClLdtChangePageSelector(int32_t entry_number,
                                   NaClLdtDescriptorType type,
                                   int read_exec_only,
                                   void* base_addr,
                                   uint32_t size_in_pages);

uint16_t NaClLdtChangeByteSelector(int32_t entry_number,
                                   NaClLdtDescriptorType type,
                                   int read_exec_only,
                                   void* base_addr,
                                   uint32_t size_in_bytes);

uint16_t NaClLdtAllocateSelector(int entry_number,
                                 int size_is_in_pages,
                                 NaClLdtDescriptorType type,
                                 int read_exec_only,
                                 void* base_addr,
                                 uint32_t size_minus_one);

/*
 * NaClLdtDeleteSelector frees the LDT entry associated with a given selector.
 */
void NaClLdtDeleteSelector(uint16_t selector);

/*
 * NaClLdtPrintSelector prints the local descriptor table for the specified
 * selector.
 */
void NaClLdtPrintSelector(uint16_t selector);

EXTERN_C_END

#endif

