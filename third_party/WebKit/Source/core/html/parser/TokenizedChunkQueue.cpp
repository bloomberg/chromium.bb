// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/TokenizedChunkQueue.h"

#include "platform/RuntimeEnabledFeatures.h"
#include <algorithm>
#include <memory>

namespace blink {

TokenizedChunkQueue::TokenizedChunkQueue() {}

TokenizedChunkQueue::~TokenizedChunkQueue() {}

bool TokenizedChunkQueue::enqueue(
    std::unique_ptr<HTMLDocumentParser::TokenizedChunk> chunk) {
  m_pendingTokenCount += chunk->tokens->size();
  m_peakPendingTokenCount =
      std::max(m_peakPendingTokenCount, m_pendingTokenCount);

  bool wasEmpty = m_pendingChunks.isEmpty();
  m_pendingChunks.push_back(std::move(chunk));
  m_peakPendingChunkCount =
      std::max(m_peakPendingChunkCount, m_pendingChunks.size());

  return wasEmpty;
}

void TokenizedChunkQueue::clear() {
  m_pendingTokenCount = 0;
  m_pendingChunks.clear();
}

void TokenizedChunkQueue::takeAll(
    Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>>& vector) {
  DCHECK(vector.isEmpty());
  m_pendingChunks.swap(vector);
}

size_t TokenizedChunkQueue::peakPendingChunkCount() {
  return m_peakPendingChunkCount;
}

size_t TokenizedChunkQueue::peakPendingTokenCount() {
  return m_peakPendingTokenCount;
}

}  // namespace blink
