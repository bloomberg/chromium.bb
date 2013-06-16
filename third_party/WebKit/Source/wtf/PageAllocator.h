/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WTF_PageAllocator_h
#define WTF_PageAllocator_h

namespace WTF {

// Our granulatity of page allocation is 64KB. This is a Windows limitation,
// but we apply the same requirement for all platforms in order to keep
// things simple and consistent.
// We term these 64KB allocations "super pages". They're just a clump of
// underlying 4KB system pages.
static const size_t kSuperPageSize = 1 << 16; // 64KB
static const size_t kSuperPageOffsetMask = kSuperPageSize - 1;
static const size_t kSuperPageBaseMask = ~kSuperPageOffsetMask;

// All Blink-supported systems have 4096 sized system pages and can handle
// permissions and commit / decommit at this granularity.
static const size_t kSystemPageSize = 4096;
static const size_t kSystemPageOffsetMask = kSystemPageSize - 1;

static const size_t kNumSystemPagesPerSuperPage = kSuperPageSize / kSystemPageSize;

// Allocate one or more super pages. Addresses in the range will be readable and
// writeable but not executable.
// The requested address is just a hint; the actual address returned may
// differ. The returned address will be aligned to kSuperPageSize.
// len is in bytes, and must be a multiple of kSuperPageSize.
// This call will exit the process if the allocation cannot be satisfied.
void* allocSuperPages(void* addr, size_t len);

// Free one or more super pages.
// addr and len must match a previous call to allocPages().
void freeSuperPages(void* addr, size_t len);

// Mark one or more system pages as being inaccessible. This is not reversible.
// Subsequently accessing any address in the range will fault, the addresses
// will not be re-used by future allocations.
// len must be a multiple of kSystemPageSize bytes.
void setSystemPagesInaccessible(void* addr, size_t len);

// Decommit one or more system pages. Decommitted means that the physical memory
// is released to the system, but the virtual address space remains reserved.
// System pages are re-committed by writing to them.
// Clients should not make any assumptions about the contents of decommitted
// system pages, before or after they write to the page. The only guarantee
// provided is that the contents of the system page will be deterministic again // after writing to it. In particlar note that system pages are not guaranteed
// to be zero-filled upon re-commit.
// len must be a multiple of kSystemPageSize bytes.
void decommitSystemPages(void* addr, size_t len);

// Returns a suitable pointer for starting to allocate super pages.
// The pointer is not guaranteed to be "unused", but does represent an address
// that has a good chance of being unused. The pointer is also randomized to
// provide reasonable ASLR.
char* getRandomSuperPageBase();

} // namespace WTF

#endif // WTF_PageAllocator_h
