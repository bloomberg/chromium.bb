/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef AddressSanitizer_h
#define AddressSanitizer_h

// The following API isn't exposed by SyzyASan (current version of ASan on
// Windows).
// FIXME: Add Windows support here.
#if defined(ADDRESS_SANITIZER) && !OS(WIN)
extern "C" {
    // Marks memory region [addr, addr+size) as unaddressable.
    // This memory must be previously allocated by the user program. Accessing
    // addresses in this region from instrumented code is forbidden until
    // this region is unpoisoned. This function is not guaranteed to poison
    // the whole region - it may poison only subregion of [addr, addr+size) due
    // to ASan alignment restrictions.
    // Method is NOT thread-safe in the sense that no two threads can
    // (un)poison memory in the same memory region simultaneously.
    void __asan_poison_memory_region(void const volatile*, size_t);
    // Marks memory region [addr, addr+size) as addressable.
    // This memory must be previously allocated by the user program. Accessing
    // addresses in this region is allowed until this region is poisoned again.
    // This function may unpoison a superregion of [addr, addr+size) due to
    // ASan alignment restrictions.
    // Method is NOT thread-safe in the sense that no two threads can
    // (un)poison memory in the same memory region simultaneously.
    void __asan_unpoison_memory_region(void const volatile*, size_t);

    // User code should use macros instead of functions.
#define ASAN_POISON_MEMORY_REGION(addr, size)   \
    __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
    __asan_unpoison_memory_region((addr), (size))
#define NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
}
#else
#define ASAN_POISON_MEMORY_REGION(addr, size)   \
    ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
    ((void)(addr), (void)(size))
#define NO_SANITIZE_ADDRESS
#endif

const size_t asanMagic = 0xabefeed0;
const size_t asanDeferMemoryReuseCount = 2;
const size_t asanDeferMemoryReuseMask = 0x3;

#endif
