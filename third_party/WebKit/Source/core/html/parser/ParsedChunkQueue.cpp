// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/ParsedChunkQueue.h"

#include <algorithm>
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

    m_pendingTokenCount += chunk->tokens->size();
    m_peakPendingTokenCount = std::max(m_peakPendingTokenCount, m_pendingTokenCount);

    bool wasEmpty = m_pendingChunks.isEmpty();
    m_pendingChunks.append(std::move(chunk));
    m_peakPendingChunkCount = std::max(m_peakPendingChunkCount, m_pendingChunks.size());

    return wasEmpty;
}

void ParsedChunkQueue::clear()
{
    MutexLocker locker(m_mutex);

    m_pendingTokenCount = 0;
    m_pendingChunks.clear();
}

void ParsedChunkQueue::takeAll(Vector<std::unique_ptr<HTMLDocumentParser::ParsedChunk>>& vector)
{
    MutexLocker locker(m_mutex);

    ASSERT(vector.isEmpty());
    m_pendingChunks.swap(vector);
}

size_t ParsedChunkQueue::peakPendingChunkCount()
{
    MutexLocker locker(m_mutex);
    return m_peakPendingChunkCount;
}

size_t ParsedChunkQueue::peakPendingTokenCount()
{
    MutexLocker locker(m_mutex);
    return m_peakPendingTokenCount;
}

} // namespace blink
