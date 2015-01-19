// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/Body.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/UseCounter.h"
#include "core/streams/UnderlyingSource.h"
#include "modules/fetch/BodyStreamBuffer.h"

namespace blink {

class Body::BlobHandleReceiver final : public BodyStreamBuffer::BlobHandleCreatorClient {
public:
    explicit BlobHandleReceiver(Body* body)
        : m_body(body)
    {
    }
    void didCreateBlobHandle(PassRefPtr<BlobDataHandle> handle) override
    {
        ASSERT(m_body);
        m_body->readAsyncFromBlob(handle);
        m_body = nullptr;
    }
    void didFail(PassRefPtrWillBeRawPtr<DOMException> exception) override
    {
        ASSERT(m_body);
        m_body->didBlobHandleReceiveError(exception);
        m_body = nullptr;
    }
    void trace(Visitor* visitor) override
    {
        BodyStreamBuffer::BlobHandleCreatorClient::trace(visitor);
        visitor->trace(m_body);
    }
private:
    Member<Body> m_body;
};

class Body::ReadableStreamSource : public GarbageCollectedFinalized<ReadableStreamSource>, public UnderlyingSource {
    USING_GARBAGE_COLLECTED_MIXIN(ReadableStreamSource);
public:
    ReadableStreamSource(Body* body) : m_body(body) { }
    ~ReadableStreamSource() override { }
    void pullSource() override { m_body->pullSource(); }

    ScriptPromise cancelSource(ScriptState* scriptState, ScriptValue reason) override
    {
        return ScriptPromise();
    }

    void trace(Visitor* visitor) override
    {
        visitor->trace(m_body);
        UnderlyingSource::trace(visitor);
    }

private:
    Member<Body> m_body;
};

void Body::pullSource()
{
    if (!m_streamAccessed) {
        // We do not download data unless the user explicitly uses the
        // ReadableStream object in order to avoid performance regression,
        // because currently Chrome cannot handle Streams efficiently
        // especially with ServiceWorker or Blob.
        return;
    }
    if (m_bodyUsed) {
        m_stream->error(DOMException::create(InvalidStateError, "The stream is locked."));
        return;
    }
    ASSERT(!m_loader);
    if (buffer()) {
        // If the body has a body buffer, we read all data from the buffer and
        // create a blob and then put the data from the blob to |m_stream|.
        // FIXME: Put the data directry from the buffer.
        buffer()->readAllAndCreateBlobHandle(contentTypeForBuffer(), new BlobHandleReceiver(this));
        return;
    }
    RefPtr<BlobDataHandle> blobHandle = blobDataHandle();
    if (!blobHandle.get()) {
        blobHandle = BlobDataHandle::create(BlobData::create(), 0);
    }
    readAsyncFromBlob(blobHandle);
}

ScriptPromise Body::readAsync(ScriptState* scriptState, ResponseType type)
{
    if (m_bodyUsed)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Already read"));

    // When the main thread sends a V8::TerminateExecution() signal to a worker
    // thread, any V8 API on the worker thread starts returning an empty
    // handle. This can happen in Body::readAsync. To avoid the situation, we
    // first check the ExecutionContext and return immediately if it's already
    // gone (which means that the V8::TerminateExecution() signal has been sent
    // to this worker thread).
    ExecutionContext* executionContext = scriptState->executionContext();
    if (!executionContext)
        return ScriptPromise();

    m_bodyUsed = true;
    m_responseType = type;

    ASSERT(!m_resolver);
    m_resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = m_resolver->promise();

    if (m_streamAccessed) {
        // 'body' attribute was accessed and the stream source started pulling.
        switch (m_stream->state()) {
        case ReadableStream::Readable:
            readAllFromStream();
            return promise;
        case ReadableStream::Waiting:
            // m_loader is working and m_resolver will be resolved when it
            // ends.
            return promise;
        case ReadableStream::Closed:
        case ReadableStream::Errored:
            m_resolver->resolve(m_stream->closed(scriptState).v8Value());
            return promise;
            break;
        }
        ASSERT_NOT_REACHED();
        return promise;
    }

    if (buffer()) {
        buffer()->readAllAndCreateBlobHandle(contentTypeForBuffer(), new BlobHandleReceiver(this));
        return promise;
    }
    readAsyncFromBlob(blobDataHandle());
    return promise;
}

void Body::readAsyncFromBlob(PassRefPtr<BlobDataHandle> handle)
{
    if (m_streamAccessed) {
        FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsArrayBuffer;
        m_loader = adoptPtr(new FileReaderLoader(readType, this));
        m_loader->start(executionContext(), handle);
        return;
    }
    FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsText;
    RefPtr<BlobDataHandle> blobHandle = handle;
    if (!blobHandle.get()) {
        blobHandle = BlobDataHandle::create(BlobData::create(), 0);
    }
    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        readType = FileReaderLoader::ReadAsArrayBuffer;
        break;
    case ResponseAsBlob:
        if (blobHandle->size() != kuint64max) {
            // If the size of |blobHandle| is set correctly, creates Blob from
            // it.
            m_resolver->resolve(Blob::create(blobHandle));
            m_resolver.clear();
            return;
        }
        // If the size is not set, read as ArrayBuffer and create a new blob to
        // get the size.
        // FIXME: This workaround is not good for performance.
        // When we will stop using Blob as a base system of Body to support
        // stream, this problem should be solved.
        readType = FileReaderLoader::ReadAsArrayBuffer;
        break;
    case ResponseAsFormData:
        // FIXME: Implement this.
        ASSERT_NOT_REACHED();
        break;
    case ResponseAsJSON:
    case ResponseAsText:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_loader = adoptPtr(new FileReaderLoader(readType, this));
    m_loader->start(m_resolver->scriptState()->executionContext(), blobHandle);

    return;
}

void Body::readAllFromStream()
{
    ASSERT(m_stream->state() == ReadableStream::Readable);
    ASSERT(m_stream->isDraining());

    Deque<std::pair<RefPtr<DOMArrayBuffer>, size_t>> queue;
    m_stream->read(queue);
    ASSERT(m_stream->state() == ReadableStream::Closed);
    // With the current loading mechanism, the data is loaded atomically.
    ASSERT(queue.size() == 1);
    didFinishLoadingViaStream(queue[0].first);
    m_resolver.clear();
    m_stream->close();
}

ScriptPromise Body::arrayBuffer(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsArrayBuffer);
}

ScriptPromise Body::blob(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsBlob);
}

ScriptPromise Body::formData(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsFormData);
}

ScriptPromise Body::json(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsJSON);
}

ScriptPromise Body::text(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsText);
}

ReadableStream* Body::body()
{
    UseCounter::count(executionContext(), UseCounter::FetchBodyStream);
    if (!m_streamAccessed) {
        m_streamAccessed = true;
        if (m_stream->isPulling()) {
            // The stream has been pulling, but the source ignored the
            // instruction because it didn't know the user wanted to use the
            // ReadableStream interface. Now it knows the user does, so have
            // the source start pulling.
            m_streamSource->pullSource();
        }
    }
    return m_stream;
}

bool Body::bodyUsed() const
{
    return m_bodyUsed;
}

void Body::setBodyUsed()
{
    m_bodyUsed = true;
}

bool Body::streamAccessed() const
{
    return m_streamAccessed;
}

void Body::stop()
{
    // Canceling the load will call didFail which will remove the resolver.
    if (m_loader)
        m_loader->cancel();
}

bool Body::hasPendingActivity() const
{
    if (m_resolver)
        return true;
    if (m_streamAccessed && (m_stream->state() == ReadableStream::Readable || m_stream->state() == ReadableStream::Waiting))
        return true;
    return false;
}

void Body::trace(Visitor* visitor)
{
    visitor->trace(m_resolver);
    visitor->trace(m_stream);
    visitor->trace(m_streamSource);
    ActiveDOMObject::trace(visitor);
}

Body::Body(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_bodyUsed(false)
    , m_streamAccessed(false)
    , m_responseType(ResponseType::ResponseUnknown)
    , m_streamSource(new ReadableStreamSource(this))
    , m_stream(new ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>(context, m_streamSource))
{
    m_stream->didSourceStart();
}

Body::Body(const Body& copy_from)
    : ActiveDOMObject(copy_from.lifecycleContext())
    , m_bodyUsed(copy_from.bodyUsed())
    , m_responseType(ResponseType::ResponseUnknown)
    , m_streamSource(new ReadableStreamSource(this))
    , m_stream(new ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>(copy_from.executionContext(), m_streamSource))
{
    m_stream->didSourceStart();
}

void Body::resolveJSON(const String& string)
{
    ASSERT(m_responseType == ResponseAsJSON);
    ScriptState::Scope scope(m_resolver->scriptState());
    v8::Isolate* isolate = m_resolver->scriptState()->isolate();
    v8::Local<v8::String> inputString = v8String(isolate, string);
    v8::TryCatch trycatch;
    v8::Local<v8::Value> parsed = v8::JSON::Parse(inputString);
    if (parsed.IsEmpty()) {
        if (trycatch.HasCaught())
            m_resolver->reject(trycatch.Exception());
        else
            m_resolver->reject(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "JSON parse error")));
        return;
    }
    m_resolver->resolve(parsed);
}

// FileReaderLoaderClient functions.
void Body::didStartLoading() { }
void Body::didReceiveData() { }
void Body::didFinishLoading()
{
    if (!executionContext() || executionContext()->activeDOMObjectsAreStopped())
        return;

    if (m_streamAccessed) {
        didFinishLoadingViaStream(m_loader->arrayBufferResult());
        m_resolver.clear();
        m_stream->close();
        return;
    }

    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        m_resolver->resolve(m_loader->arrayBufferResult());
        break;
    case ResponseAsBlob: {
        ASSERT(blobDataHandle()->size() == kuint64max);
        OwnPtr<BlobData> blobData = BlobData::create();
        RefPtr<DOMArrayBuffer> buffer = m_loader->arrayBufferResult();
        blobData->appendBytes(buffer->data(), buffer->byteLength());
        const size_t length = blobData->length();
        m_resolver->resolve(Blob::create(BlobDataHandle::create(blobData.release(), length)));
        break;
    }
    case ResponseAsFormData:
        ASSERT_NOT_REACHED();
        break;
    case ResponseAsJSON:
        resolveJSON(m_loader->stringResult());
        break;
    case ResponseAsText:
        m_resolver->resolve(m_loader->stringResult());
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    m_resolver.clear();
    m_stream->close();
}

void Body::didFinishLoadingViaStream(PassRefPtr<DOMArrayBuffer> buffer)
{
    if (!m_bodyUsed) {
        // |m_stream| is pulling.
        ASSERT(m_streamAccessed);
        m_stream->enqueue(buffer);
        return;
    }

    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        m_resolver->resolve(buffer);
        break;
    case ResponseAsBlob: {
        OwnPtr<BlobData> blobData = BlobData::create();
        blobData->appendBytes(buffer->data(), buffer->byteLength());
        m_resolver->resolve(Blob::create(BlobDataHandle::create(blobData.release(), blobData->length())));
        break;
    }
    case ResponseAsFormData:
        ASSERT_NOT_REACHED();
        break;
    case ResponseAsJSON: {
        String s = String::fromUTF8(static_cast<const char*>(buffer->data()), buffer->byteLength());
        if (s.isNull())
            m_resolver->reject(DOMException::create(NetworkError, "Invalid utf-8 string"));
        else
            resolveJSON(s);
        break;
    }
    case ResponseAsText: {
        String s = String::fromUTF8(static_cast<const char*>(buffer->data()), buffer->byteLength());
        if (s.isNull())
            m_resolver->reject(DOMException::create(NetworkError, "Invalid utf-8 string"));
        else
            m_resolver->resolve(s);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

void Body::didFail(FileError::ErrorCode code)
{
    if (!executionContext() || executionContext()->activeDOMObjectsAreStopped())
        return;

    if (m_resolver) {
        // FIXME: We should reject the promise.
        m_resolver->resolve("");
        m_resolver.clear();
    }
    m_stream->error(DOMException::create(NetworkError, "network error"));
}

void Body::didBlobHandleReceiveError(PassRefPtrWillBeRawPtr<DOMException> exception)
{
    if (!m_resolver)
        return;
    m_resolver->reject(exception);
    m_resolver.clear();
}

} // namespace blink
