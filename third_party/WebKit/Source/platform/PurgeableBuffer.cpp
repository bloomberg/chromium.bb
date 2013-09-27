/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/PurgeableBuffer.h"

#include "public/platform/Platform.h"
#include "public/platform/WebDiscardableMemory.h"

#include <string.h>

namespace WebCore {

// WebDiscardableMemory allocations are a limited resource. We only use them
// when there's a reasonable amount of memory to be saved by the OS discarding
// the memory.
static size_t minimumSize = 4 * 4096;

PassOwnPtr<PurgeableBuffer> PurgeableBuffer::create(const char* data, size_t size)
{
    if (size < minimumSize)
        return nullptr;

    OwnPtr<WebKit::WebDiscardableMemory> memory = adoptPtr(WebKit::Platform::current()->allocateAndLockDiscardableMemory(size));
    if (!memory)
        return nullptr;

    return adoptPtr(new PurgeableBuffer(memory.release(), data, size));
}

PurgeableBuffer::~PurgeableBuffer()
{
    if (m_state == Locked)
        m_memory->unlock();
}

const char* PurgeableBuffer::data() const
{
    ASSERT(m_state == Locked);
    return static_cast<const char*>(m_memory->data());
}

bool PurgeableBuffer::wasPurged() const
{
    return m_state == Purged;
}

bool PurgeableBuffer::lock()
{
    ASSERT(m_state == Unlocked);
    if (!m_memory->lock()) {
        m_state = Purged;
        m_memory = nullptr;
        return false;
    }
    m_state = Locked;
    return true;
}

void PurgeableBuffer::unlock()
{
    ASSERT(m_state == Locked);
    m_memory->unlock();
    m_state = Unlocked;
}

PurgeableBuffer::PurgeableBuffer(PassOwnPtr<WebKit::WebDiscardableMemory> memory, const char* data, size_t size)
    : m_memory(memory)
    , m_size(size)
    , m_state(Locked)
{
    memcpy(m_memory->data(), data, size);
}

}
