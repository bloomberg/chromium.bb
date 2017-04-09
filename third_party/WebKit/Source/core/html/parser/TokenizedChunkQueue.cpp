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

bool TokenizedChunkQueue::Enqueue(
    std::unique_ptr<HTMLDocumentParser::TokenizedChunk> chunk) {
  pending_token_count_ += chunk->tokens->size();
  peak_pending_token_count_ =
      std::max(peak_pending_token_count_, pending_token_count_);

  bool was_empty = pending_chunks_.IsEmpty();
  pending_chunks_.push_back(std::move(chunk));
  peak_pending_chunk_count_ =
      std::max(peak_pending_chunk_count_, pending_chunks_.size());

  return was_empty;
}

void TokenizedChunkQueue::Clear() {
  pending_token_count_ = 0;
  pending_chunks_.Clear();
}

void TokenizedChunkQueue::TakeAll(
    Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>>& vector) {
  DCHECK(vector.IsEmpty());
  pending_chunks_.Swap(vector);
}

size_t TokenizedChunkQueue::PeakPendingChunkCount() {
  return peak_pending_chunk_count_;
}

size_t TokenizedChunkQueue::PeakPendingTokenCount() {
  return peak_pending_token_count_;
}

}  // namespace blink
