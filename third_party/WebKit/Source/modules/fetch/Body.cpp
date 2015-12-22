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
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchDataLoader.h"
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
        ScriptState::Scope scope(resolver()->scriptState());
        m_resolver->reject(V8ThrowException::createTypeError(resolver()->scriptState()->isolate(), "Failed to fetch"));
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

    void didFetchDataLoadedArrayBuffer(PassRefPtr<DOMArrayBuffer> arrayBuffer) override
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
        if (!resolver()->executionContext() || resolver()->executionContext()->activeDOMObjectsAreStopped())
            return;
        ScriptState::Scope scope(resolver()->scriptState());
        v8::Isolate* isolate = resolver()->scriptState()->isolate();
        v8::Local<v8::String> inputString = v8String(isolate, string);
        v8::TryCatch trycatch(isolate);
        v8::Local<v8::Value> parsed;
        if (v8Call(v8::JSON::Parse(isolate, inputString), parsed, trycatch))
            resolver()->resolve(parsed);
        else
            resolver()->reject(trycatch.Exception());
    }
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
    if (!scriptState->executionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->executionContext(), FetchDataLoader::createLoaderAsArrayBuffer(), new BodyArrayBufferConsumer(resolver));
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
    if (!scriptState->executionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->executionContext(), FetchDataLoader::createLoaderAsBlobHandle(mimeType()), new BodyBlobConsumer(resolver));
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
    if (!scriptState->executionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->executionContext(), FetchDataLoader::createLoaderAsString(), new BodyJsonConsumer(resolver));
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
    if (!scriptState->executionContext())
        return ScriptPromise();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    promise = resolver->promise();
    if (bodyBuffer()) {
        bodyBuffer()->startLoading(scriptState->executionContext(), FetchDataLoader::createLoaderAsString(), new BodyTextConsumer(resolver));
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
    UseCounter::count(executionContext(), UseCounter::FetchBodyStream);
    return body();
}

bool Body::bodyUsed()
{
    return body() && body()->isDisturbed();
}

bool Body::isBodyLocked()
{
    return body() && body()->isLocked();
}

bool Body::hasPendingActivity() const
{
    if (executionContext()->activeDOMObjectsAreStopped())
        return false;
    if (!bodyBuffer())
        return false;
    return bodyBuffer()->hasPendingActivity();
}

Body::Body(ExecutionContext* context) : ActiveDOMObject(context), m_opaque(false)
{
    suspendIfNeeded();
}

ScriptPromise Body::rejectInvalidConsumption(ScriptState* scriptState)
{
    if (m_opaque)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "The body is opaque."));
    if (isBodyLocked() || bodyUsed())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Already read"));
    return ScriptPromise();
}

} // namespace blink
