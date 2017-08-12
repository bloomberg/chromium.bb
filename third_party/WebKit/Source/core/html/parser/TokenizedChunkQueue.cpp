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
  bool was_empty = pending_chunks_.IsEmpty();
  pending_chunks_.push_back(std::move(chunk));
  return was_empty;
}

void TokenizedChunkQueue::Clear() {
  pending_chunks_.clear();
}

void TokenizedChunkQueue::TakeAll(
    Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>>& vector) {
  DCHECK(vector.IsEmpty());
  pending_chunks_.swap(vector);
}

}  // namespace blink
