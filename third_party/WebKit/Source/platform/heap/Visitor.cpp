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

#include "config.h"
#include "platform/heap/Visitor.h"

#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace blink {

// GCInfo indices start from 1 for heap objects, with 0 being treated
// specially as the index for freelist entries and large heap objects.
int GCInfoTable::s_gcInfoIndex = 0;

size_t GCInfoTable::s_gcInfoTableSize = 0;
GCInfo const** s_gcInfoTable = nullptr;

void GCInfoTable::ensureGCInfoIndex(const GCInfo* gcInfo, size_t* gcInfoIndexSlot)
{
    ASSERT(gcInfo);
    ASSERT(gcInfoIndexSlot);
    // Keep a global GCInfoTable lock while allocating a new slot.
    AtomicallyInitializedStaticReference(Mutex, mutex, new Mutex);
    MutexLocker locker(mutex);

    // If more than one thread ends up allocating a slot for
    // the same GCInfo, have later threads reuse the slot
    // allocated by the first.
    if (*gcInfoIndexSlot)
        return;

    int index = ++s_gcInfoIndex;
    size_t gcInfoIndex = static_cast<size_t>(index);
    RELEASE_ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
    if (gcInfoIndex >= s_gcInfoTableSize)
        resize();

    s_gcInfoTable[gcInfoIndex] = gcInfo;
    releaseStore(reinterpret_cast<int*>(gcInfoIndexSlot), index);
}

void GCInfoTable::resize()
{
    // (Light) experimentation suggests that Blink doesn't need
    // more than this while handling content on popular web properties.
    const size_t initialSize = 512;

    size_t newSize = s_gcInfoTableSize ? 2 * s_gcInfoTableSize : initialSize;
    ASSERT(newSize < GCInfoTable::maxIndex);
    s_gcInfoTable = reinterpret_cast<GCInfo const**>(realloc(s_gcInfoTable, newSize * sizeof(GCInfo)));
    ASSERT(s_gcInfoTable);
    memset(reinterpret_cast<uint8_t*>(s_gcInfoTable) + s_gcInfoTableSize * sizeof(GCInfo), 0, (newSize - s_gcInfoTableSize) * sizeof(GCInfo));
    s_gcInfoTableSize = newSize;
}

void GCInfoTable::init()
{
    RELEASE_ASSERT(!s_gcInfoTable);
    resize();
}

void GCInfoTable::shutdown()
{
    free(s_gcInfoTable);
    s_gcInfoTable = nullptr;
}

StackFrameDepth* Visitor::m_stackFrameDepth = nullptr;

#if ENABLE(ASSERT)
void assertObjectHasGCInfo(const void* payload, size_t gcInfoIndex)
{
    HeapObjectHeader::fromPayload(payload)->checkHeader();
#if !defined(COMPONENT_BUILD)
    // On component builds we cannot compare the gcInfos as they are statically
    // defined in each of the components and hence will not match.
    BasePage* page = pageFromObject(payload);
    ASSERT(page->orphaned() || HeapObjectHeader::fromPayload(payload)->gcInfoIndex() == gcInfoIndex);
#endif
}
#endif

}
