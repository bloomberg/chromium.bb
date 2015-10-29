// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParsedChunkQueue_h
#define ParsedChunkQueue_h

#include "core/html/parser/HTMLDocumentParser.h"
#include "wtf/Deque.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

namespace blink {

// ParsedChunkQueue is used to transfer parsed HTML token chunks
// from BackgroundHTMLParser thread to Blink main thread without
// spamming task queue.
// ParsedChunkQueue is accessed from both BackgroundHTMLParser
// thread (producer) and Blink main thread (consumer).
// Access to the backend queue vector is protected by a mutex.
// If enqueue is done against empty queue, BackgroundHTMLParser
// thread kicks a consumer task on Blink main thread.
class ParsedChunkQueue : public ThreadSafeRefCounted<ParsedChunkQueue> {
public:
    static PassRefPtr<ParsedChunkQueue> create()
    {
        return adoptRef(new ParsedChunkQueue);
    }

    ~ParsedChunkQueue();

    bool enqueue(PassOwnPtr<HTMLDocumentParser::ParsedChunk>);
    void clear();

    void takeAll(Vector<OwnPtr<HTMLDocumentParser::ParsedChunk>>&);

private:
    ParsedChunkQueue();

    Mutex m_mutex;
    Vector<OwnPtr<HTMLDocumentParser::ParsedChunk>> m_pendingChunks;
};

} // namespace blink

#endif
