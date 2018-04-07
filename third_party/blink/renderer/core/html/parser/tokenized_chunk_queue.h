// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_TOKENIZED_CHUNK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_TOKENIZED_CHUNK_QUEUE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// TokenizedChunkQueue is used to transfer parsed HTML token chunks
// from BackgroundHTMLParser thread to Blink main thread without
// spamming task queue.
// TokenizedChunkQueue is accessed from both BackgroundHTMLParser
// thread (producer) and Blink main thread (consumer).
// Access to the backend queue vector is protected by a mutex.
// If enqueue is done against empty queue, BackgroundHTMLParser
// thread kicks a consumer task on Blink main thread.
class TokenizedChunkQueue : public ThreadSafeRefCounted<TokenizedChunkQueue> {
 public:
  static scoped_refptr<TokenizedChunkQueue> Create() {
    return base::AdoptRef(new TokenizedChunkQueue);
  }

  ~TokenizedChunkQueue();

  bool Enqueue(std::unique_ptr<HTMLDocumentParser::TokenizedChunk>);
  void Clear();

  void TakeAll(Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>>&);

 private:
  TokenizedChunkQueue();

  Vector<std::unique_ptr<HTMLDocumentParser::TokenizedChunk>> pending_chunks_;
};

}  // namespace blink

#endif
