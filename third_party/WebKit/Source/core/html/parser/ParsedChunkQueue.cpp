// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/ParsedChunkQueue.h"

#include <memory>

namespace blink {

ParsedChunkQueue::ParsedChunkQueue()
{
}

ParsedChunkQueue::~ParsedChunkQueue()
{
}

bool ParsedChunkQueue::enqueue(std::unique_ptr<HTMLDocumentParser::ParsedChunk> chunk)
{
    MutexLocker locker(m_mutex);

    bool wasEmpty = m_pendingChunks.isEmpty();
    m_pendingChunks.append(std::move(chunk));
    return wasEmpty;
}

void ParsedChunkQueue::clear()
{
    MutexLocker locker(m_mutex);

    m_pendingChunks.clear();
}

void ParsedChunkQueue::takeAll(Vector<std::unique_ptr<HTMLDocumentParser::ParsedChunk>>& vector)
{
    MutexLocker locker(m_mutex);

    ASSERT(vector.isEmpty());
    m_pendingChunks.swap(vector);
}

} // namespace blink
