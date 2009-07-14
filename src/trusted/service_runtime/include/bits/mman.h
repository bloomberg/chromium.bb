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
 * NaCl Service Runtime API.
 */


#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_BITS_MMAN_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_BITS_MMAN_H_

#define NACL_ABI_PROT_READ      0x1   /* Page can be read.  */
#define NACL_ABI_PROT_WRITE     0x2   /* Page can be written.  */
#define NACL_ABI_PROT_EXEC      0x4   /* Page can be executed.  */
#define NACL_ABI_PROT_NONE      0x0   /* Page can not be accessed.  */

#define NACL_ABI_MAP_SHARED     0x01  /* Share changes.  */
#define NACL_ABI_MAP_PRIVATE    0x02  /* Changes are private.  */
#define NACL_ABI_MAP_FIXED      0x10  /* Interpret addr exactly.  */
#define NACL_ABI_MAP_ANONYMOUS  0x20  /* Don't use a file.  */
#define NACL_ABI_MAP_FAILED     ((void *) -1)

#endif
