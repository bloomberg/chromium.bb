// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BodyStreamBuffer_h
#define BodyStreamBuffer_h

#include "core/dom/DOMException.h"
#include "core/streams/ReadableByteStream.h"
#include "core/streams/ReadableByteStreamReader.h"
#include "core/streams/UnderlyingSource.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class EncodedFormData;
class ExecutionContext;

class MODULES_EXPORT BodyStreamBuffer final : public GarbageCollectedFinalized<BodyStreamBuffer>, public UnderlyingSource, public WebDataConsumerHandle::Client {
    WTF_MAKE_NONCOPYABLE(BodyStreamBuffer);
    USING_GARBAGE_COLLECTED_MIXIN(BodyStreamBuffer);
public:
    // |handle| cannot be null and cannot be locked.
    explicit BodyStreamBuffer(PassOwnPtr<FetchDataConsumerHandle> /* handle */);

    ReadableByteStream* stream() { return m_stream; }

    // Callable only when not locked.
    PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy);
    PassRefPtr<EncodedFormData> drainAsFormData();

    // Callable only when not locked. Returns a non-null handle.
    // Note: There is a case that calling |lock| doesn't make the buffer
    // locked. |unlock| should be called even in such cases when a user finishes
    // to use the returned handle, in order to maintain hasPendingActivity().
    PassOwnPtr<FetchDataConsumerHandle> lock(ExecutionContext*);

    // This function will lock |this| object. |client| cannot be null.
    void startLoading(ExecutionContext*, FetchDataLoader*, FetchDataLoader::Client* /* client */);

    bool isLocked() const { return m_stream->isLocked(); }
    bool hasPendingActivity() const { return isLocked() || m_lockLevel > 0; }

    // UnderlyingSource
    void pullSource() override;
    ScriptPromise cancelSource(ScriptState*, ScriptValue reason) override;

    // WebDataConsumerHandle::Client
    void didGetReadable() override;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_stream);
        visitor->trace(m_streamReader);
        visitor->trace(m_loaders);
        UnderlyingSource::trace(visitor);
    }

private:
    class LoaderHolder;
    enum EndLoadingMode {
        EndLoadingDone,
        EndLoadingErrored,
    };
    void close();
    void error();
    void processData();
    void unlock();
    void endLoading(FetchDataLoader::Client*, EndLoadingMode);

    OwnPtr<FetchDataConsumerHandle> m_handle;
    OwnPtr<FetchDataConsumerHandle::Reader> m_reader;
    Member<ReadableByteStream> m_stream;
    Member<ReadableByteStreamReader> m_streamReader;
    HeapHashSet<Member<FetchDataLoader::Client>> m_loaders;
    // We need this variable because we cannot lock closed or erroed stream
    // although we should return true for hasPendingActivity() when someone
    // calls |startLoading| but the loding is not yet done.
    unsigned m_lockLevel;
    bool m_streamNeedsMore;
};

} // namespace blink

#endif // BodyStreamBuffer_h
