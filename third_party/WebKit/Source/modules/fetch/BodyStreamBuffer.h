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
    // Needed because we have to release |m_reader| promptly.
    EAGERLY_FINALIZE();
public:
    // |handle| cannot be null and cannot be locked.
    explicit BodyStreamBuffer(PassOwnPtr<FetchDataConsumerHandle> /* handle */);

    ReadableByteStream* stream() { return m_stream; }

    // Callable only when not locked.
    PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy);
    PassRefPtr<EncodedFormData> drainAsFormData();
    void startLoading(ExecutionContext*, FetchDataLoader*, FetchDataLoader::Client* /* client */);

    // Callable only when not locked. Returns a non-null handle.
    // There is no means to "return" the handle to the body buffer: Calling
    // this function locks, disturbs and closes the stream and no one will
    // not be able to get any information from the stream and this buffer after
    // that.
    PassOwnPtr<FetchDataConsumerHandle> releaseHandle(ExecutionContext*);

    bool hasPendingActivity() const;
    void stop();

    // UnderlyingSource
    void pullSource() override;
    ScriptPromise cancelSource(ScriptState*, ScriptValue reason) override;

    // WebDataConsumerHandle::Client
    void didGetReadable() override;

    bool isStreamReadable() const;
    bool isStreamClosed() const;
    bool isStreamErrored() const;
    bool isStreamLocked() const;
    bool isStreamDisturbed() const;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_stream);
        visitor->trace(m_loader);
        UnderlyingSource::trace(visitor);
    }

private:
    class LoaderClient;
    void close();
    void error();
    void processData();
    void endLoading();
    void stopLoading();

    OwnPtr<FetchDataConsumerHandle> m_handle;
    OwnPtr<FetchDataConsumerHandle::Reader> m_reader;
    Member<ReadableByteStream> m_stream;
    // We need this member to keep it alive while loading.
    Member<FetchDataLoader> m_loader;
    bool m_streamNeedsMore;
};

} // namespace blink

#endif // BodyStreamBuffer_h
