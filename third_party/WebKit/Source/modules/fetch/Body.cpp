// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Body.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/frame/UseCounter.h"
#include "core/streams/ReadableStreamController.h"
#include "core/streams/ReadableStreamOperations.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchDataLoader.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

namespace {

class BodyConsumerBase : public GarbageCollectedFinalized<BodyConsumerBase>, public FetchDataLoader::Client {
    WTF_MAKE_NONCOPYABLE(BodyConsumerBase);
    USING_GARBAGE_COLLECTED_MIXIN(BodyConsumerBase);
public:
    explicit BodyConsumerBase(ScriptPromiseResolver* resolver) : m_resolver(resolver) {}
    ScriptPromiseResolver* resolver() { return m_resolver; }
    void didFetchDataLoadFailed() override
    {
        ScriptState::Scope scope(resolver()->getScriptState());
        m_resolver->reject(V8ThrowException::createTypeError(resolver()->getScriptState()->isolate(), "Failed to fetch"));
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_resolver);
        FetchDataLoader::Client::trace(visitor);
    }

private:
    Member<ScriptPromiseResolver> m_resolver;
};

class BodyBlobConsumer final : public BodyConsumerBase {
    WTF_MAKE_NONCOPYABLE(BodyBlobConsumer);
public:
    explicit BodyBlobConsumer(ScriptPromiseResolver* resolver) : BodyConsumerBase(resolver) {}

    void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
    {
        resolver()->resolve(Blob::create(blobDataHandle));
    }
};

class BodyArrayBufferConsumer final : public BodyConsumerBase {
    WTF_MAKE_NONCOPYABLE(BodyArrayBufferConsumer);
public:
    explicit BodyArrayBufferConsumer(ScriptPromiseResolver* resolver) : BodyConsumerBase(resolver) {}

    void didFetchDataLoadedArrayBuffer(DOMArrayBuffer* arrayBuffer) override
    {
        resolver()->resolve(arrayBuffer);
    }
};

class BodyTextConsumer final : public BodyConsumerBase {
    WTF_MAKE_NONCOPYABLE(BodyTextConsumer);
public:
    explicit BodyTextConsumer(ScriptPromiseResolver* resolver) : BodyConsumerBase(resolver) {}

    void didFetchDataLoadedString(const String& string) override
    {
        resolver()->resolve(string);
    }
};

class BodyJsonConsumer final : public BodyConsumerBase {
    WTF_MAKE_NONCOPYABLE(BodyJsonConsumer);
public:
    explicit BodyJsonConsumer(ScriptPromiseResolver* resolver) : BodyConsumerBase(resolver) {}

    void didFetchDataLoadedString(const String& string) override
    {
        if (!resolver()->getExecutionContext() || resolver()->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        ScriptState::Scope scope(resolver()->getScriptState());
        v8::Isolate* isolate = resolver()->getScriptState()->isolate();
        v8::Local<v8::String> inputString = v8String(isolate, string);
        v8::TryCatch trycatch(isolate);
        v8::Local<v8::Value> parsed;
        if (v8Call(v8::JSON::Parse(isolate, inputString), parsed, trycatch))
            resolver()->resolve(parsed);
        else
            resolver()->reject(trycatch.Exception());
    }
};

class UnderlyingSourceFromDataConsumerHandle final : public UnderlyingSourceBase, public WebDataConsumerHandle::Client {
    EAGERLY_FINALIZE();
    DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();
public:
    UnderlyingSourceFromDataConsumerHandle(ScriptState* scriptState, PassOwnPtr<WebDataConsumerHandle> handle)
        : UnderlyingSourceBase(scriptState)
        , m_scriptState(scriptState)
        , m_reader(handle->obtainReader(this))
    {
    }

    ScriptPromise pull(ScriptState* scriptState) override
    {
        didGetReadable();
        return ScriptPromise::castUndefined(scriptState);
    }

    ScriptPromise cancel(ScriptState* scriptState, ScriptValue reason) override
    {
        m_reader = nullptr;
        return ScriptPromise::castUndefined(scriptState);
    }

    void didGetReadable() override
    {
        while (controller()->desiredSize() > 0) {
            size_t available = 0;
            const void* buffer = nullptr;
            WebDataConsumerHandle::Result result = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &available);
            if (result == WebDataConsumerHandle::Ok) {
                RefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::create(available, 1);
                memcpy(arrayBuffer->data(), buffer, available);
                m_reader->endRead(available);
                controller()->enqueue(DOMUint8Array::create(arrayBuffer.release(), 0, available));
            } else if (result == WebDataConsumerHandle::ShouldWait) {
                break;
            } else if (result == WebDataConsumerHandle::Done) {
                m_reader = nullptr;
                controller()->close();
                break;
            } else {
                m_reader = nullptr;
                controller()->error(V8ThrowException::createTypeError(m_scriptState->isolate(), "Network error"));
                break;
            }
        }
    }

private:
    RefPtr<ScriptState> m_scriptState;
    OwnPtr<WebDataConsumerHandle::Reader> m_reader;
};

} // namespace

ScriptPromise Body::arrayBuffer(ScriptState* scriptState)
{
    ScriptPromise promise = rejectInvalidConsumption(scriptState);
    if (!promise.isEmpty())
        return promise;

    // When the main thread sends a V8::TerminateExecution() signal to a worker
    // thread, any V8 API on the worker thread starts returning an empty
    // handle. This can happen in this function. To avoid the situation, we
    // first check the ExecutionContext and return immediately if it's already
    // gone (which means that the V8::TerminateExecution() signal has been sent
    // to this worker thread).
    if (!scriptState->getExecutionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->getExecutionContext(), FetchDataLoader::createLoaderAsArrayBuffer(), new BodyArrayBufferConsumer(resolver));
    } else {
        resolver->resolve(DOMArrayBuffer::create(0u, 1));
    }
    return promise;
}

ScriptPromise Body::blob(ScriptState* scriptState)
{
    ScriptPromise promise = rejectInvalidConsumption(scriptState);
    if (!promise.isEmpty())
        return promise;

    // See above comment.
    if (!scriptState->getExecutionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->getExecutionContext(), FetchDataLoader::createLoaderAsBlobHandle(mimeType()), new BodyBlobConsumer(resolver));
    } else {
        OwnPtr<BlobData> blobData = BlobData::create();
        blobData->setContentType(mimeType());
        resolver->resolve(Blob::create(BlobDataHandle::create(blobData.release(), 0)));
    }
    return promise;

}

ScriptPromise Body::json(ScriptState* scriptState)
{
    ScriptPromise promise = rejectInvalidConsumption(scriptState);
    if (!promise.isEmpty())
        return promise;

    // See above comment.
    if (!scriptState->getExecutionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->getExecutionContext(), FetchDataLoader::createLoaderAsString(), new BodyJsonConsumer(resolver));
    } else {
        resolver->reject(V8ThrowException::createSyntaxError(scriptState->isolate(), "Unexpected end of input"));
    }
    return promise;
}

ScriptPromise Body::text(ScriptState* scriptState)
{
    ScriptPromise promise = rejectInvalidConsumption(scriptState);
    if (!promise.isEmpty())
        return promise;

    // See above comment.
    if (!scriptState->getExecutionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->getExecutionContext(), FetchDataLoader::createLoaderAsString(), new BodyTextConsumer(resolver));
    } else {
        resolver->resolve(String());
    }
    return promise;
}

ReadableByteStream* Body::body()
{
    return bodyBuffer() ? bodyBuffer()->stream() : nullptr;
}

ReadableByteStream* Body::bodyWithUseCounter()
{
    UseCounter::count(getExecutionContext(), UseCounter::FetchBodyStream);
    return body();
}

ScriptValue Body::v8ExtraStreamBody(ScriptState* scriptState)
{
    if (bodyUsed() || isBodyLocked() || !bodyBuffer())
        return ScriptValue(scriptState, v8::Undefined(scriptState->isolate()));
    OwnPtr<FetchDataConsumerHandle> handle = bodyBuffer()->releaseHandle(scriptState->getExecutionContext());
    return ReadableStreamOperations::createReadableStream(scriptState,
        new UnderlyingSourceFromDataConsumerHandle(scriptState, handle.release()),
        ScriptValue(scriptState, v8::Undefined(scriptState->isolate())));
}

bool Body::bodyUsed()
{
    return bodyBuffer() && bodyBuffer()->isStreamDisturbed();
}

bool Body::isBodyLocked()
{
    return bodyBuffer() && bodyBuffer()->isStreamLocked();
}

bool Body::hasPendingActivity() const
{
    if (getExecutionContext()->activeDOMObjectsAreStopped())
        return false;
    if (!bodyBuffer())
        return false;
    return bodyBuffer()->hasPendingActivity();
}

Body::Body(ExecutionContext* context)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
{
    suspendIfNeeded();
}

ScriptPromise Body::rejectInvalidConsumption(ScriptState* scriptState)
{
    if (isBodyLocked() || bodyUsed())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Already read"));
    return ScriptPromise();
}

} // namespace blink
