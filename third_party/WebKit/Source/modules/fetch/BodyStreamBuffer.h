// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BodyStreamBuffer_h
#define BodyStreamBuffer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/DOMException.h"
#include "core/streams/ReadableByteStream.h"
#include "core/streams/ReadableByteStreamReader.h"
#include "core/streams/UnderlyingSource.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class EncodedFormData;
class ScriptState;

class MODULES_EXPORT BodyStreamBuffer final : public UnderlyingSourceBase, public UnderlyingSource, public WebDataConsumerHandle::Client {
    WTF_MAKE_NONCOPYABLE(BodyStreamBuffer);
    USING_GARBAGE_COLLECTED_MIXIN(BodyStreamBuffer);
    // Needed because we have to release |m_reader| promptly.
    EAGERLY_FINALIZE();
public:
    // |handle| cannot be null and cannot be locked.
    BodyStreamBuffer(ScriptState*, PassOwnPtr<FetchDataConsumerHandle> /* handle */);

    ScriptValue stream();

    // Callable only when neither locked nor disturbed.
    PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy);
    PassRefPtr<EncodedFormData> drainAsFormData();
    void startLoading(FetchDataLoader*, FetchDataLoader::Client* /* client */);

    // Callable only when not locked. Returns a non-null handle.
    // There is no means to "return" the handle to the body buffer: Calling
    // this function locks, disturbs and closes the stream and no one will
    // not be able to get any information from the stream and this buffer after
    // that.
    PassOwnPtr<FetchDataConsumerHandle> releaseHandle();

    bool hasPendingActivity() const;
    void stop();

    // UnderlyingSource
    void pullSource() override;
    ScriptPromise cancelSource(ScriptState*, ScriptValue reason) override;

    // UnderlyingSourceBase
    ScriptPromise pull(ScriptState*) override;
    ScriptPromise cancel(ScriptState*, ScriptValue reason) override;

    // WebDataConsumerHandle::Client
    void didGetReadable() override;

    bool isStreamReadable();
    bool isStreamClosed();
    bool isStreamErrored();
    bool isStreamLocked();
    bool isStreamDisturbed();
    void setDisturbed();

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_stream);
        visitor->trace(m_loader);
        UnderlyingSourceBase::trace(visitor);
        UnderlyingSource::trace(visitor);
    }

private:
    class LoaderClient;

    void lockAndDisturb();
    void close();
    void error();
    void processData();
    void endLoading();
    void stopLoading();

    RefPtr<ScriptState> m_scriptState;
    OwnPtr<FetchDataConsumerHandle> m_handle;
    OwnPtr<FetchDataConsumerHandle::Reader> m_reader;
    Member<ReadableByteStream> m_stream;
    // We need this member to keep it alive while loading.
    Member<FetchDataLoader> m_loader;
    bool m_streamNeedsMore = false;
};

} // namespace blink

#endif // BodyStreamBuffer_h
