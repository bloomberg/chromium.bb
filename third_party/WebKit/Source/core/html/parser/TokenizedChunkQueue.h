// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TokenizedChunkQueue_h
#define TokenizedChunkQueue_h

#include <memory>
#include "core/html/parser/HTMLDocumentParser.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"

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
    return WTF::AdoptRef(new TokenizedChunkQueue);
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
