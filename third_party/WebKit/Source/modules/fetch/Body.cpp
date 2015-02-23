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
#include "core/streams/ExclusiveStreamReader.h"
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
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        BodyStreamBuffer::BlobHandleCreatorClient::trace(visitor);
        visitor->trace(m_body);
    }
private:
    Member<Body> m_body;
};

class Body::ReadableStreamSource : public BodyStreamBuffer::Observer , public UnderlyingSource, public FileReaderLoaderClient {
    USING_GARBAGE_COLLECTED_MIXIN(ReadableStreamSource);
public:
    enum State {
        Initial,
        Streaming,
        ReadingBlob,
        Closed,
        Errored,
        BodyUsed,
    };
    ReadableStreamSource(Body* body) : m_body(body), m_state(Initial), m_queueCount(0)
    {
        if (m_body->buffer()) {
            m_bodyStreamBuffer = m_body->buffer();
        } else {
            m_blobDataHandle = m_body->blobDataHandle();
            if (!m_blobDataHandle)
                m_blobDataHandle = BlobDataHandle::create(BlobData::create(), 0);
        }
    }
    ~ReadableStreamSource() override { }
    void startStream(ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer> >* stream)
    {
        ASSERT(m_state == Initial);
        ASSERT(!m_stream);
        ASSERT(stream);
        m_stream = stream;
        stream->didSourceStart();
        if (m_body->bodyUsed()) {
            m_state = BodyUsed;
            m_stream->error(DOMException::create(InvalidStateError, "The stream is locked."));
            return;
        }
        if (m_bodyStreamBuffer) {
            ASSERT(m_bodyStreamBuffer == m_body->buffer());
            m_state = Streaming;
            m_bodyStreamBuffer->registerObserver(this);
            onWrite();
            if (m_bodyStreamBuffer->hasError())
                return onError();
            if (m_bodyStreamBuffer->isClosed())
                return onClose();
        } else {
            ASSERT(m_blobDataHandle);
            m_state = ReadingBlob;
            FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsArrayBuffer;
            m_loader = adoptPtr(new FileReaderLoader(readType, this));
            m_loader->start(m_body->executionContext(), m_blobDataHandle);
        }
    }
    // Creates a new BodyStreamBuffer to drain the data.
    BodyStreamBuffer* createDrainingStream(bool* dataLost)
    {
        ASSERT(!m_drainingStreamBuffer);
        ASSERT(m_state != Initial);
        ASSERT(m_state != BodyUsed);
        ASSERT(m_stream);
        ASSERT(dataLost);
        m_drainingStreamBuffer = new BodyStreamBuffer();
        if (m_state == Errored) {
            m_drainingStreamBuffer->error(exception());
            return m_drainingStreamBuffer;
        }
        // Take back the data in |m_stream|.
        Deque<std::pair<RefPtr<DOMArrayBuffer>, size_t>> tmp_queue;
        if (m_stream->stateInternal() == ReadableStream::Readable)
            m_stream->readInternal(tmp_queue);
        *dataLost = m_queueCount != tmp_queue.size();
        while (!tmp_queue.isEmpty()) {
            std::pair<RefPtr<DOMArrayBuffer>, size_t> data = tmp_queue.takeFirst();
            m_drainingStreamBuffer->write(data.first);
        }
        if (m_state == Closed)
            m_drainingStreamBuffer->close();
        else
            m_stream->close();
        return m_drainingStreamBuffer;
    }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_body);
        visitor->trace(m_bodyStreamBuffer);
        visitor->trace(m_drainingStreamBuffer);
        visitor->trace(m_stream);
        BodyStreamBuffer::Observer::trace(visitor);
        UnderlyingSource::trace(visitor);
    }

private:
    // UnderlyingSource functions.
    void pullSource() override { }
    ScriptPromise cancelSource(ScriptState* scriptState, ScriptValue reason) override
    {
        return ScriptPromise();
    }

    // BodyStreamBuffer::Observer functions.
    void onWrite() override
    {
        ASSERT(m_state == Streaming);
        while (RefPtr<DOMArrayBuffer> buf = m_bodyStreamBuffer->read()) {
            write(buf);
        }
    }
    void onClose() override
    {
        ASSERT(m_state == Streaming);
        close();
        m_bodyStreamBuffer->unregisterObserver();
    }
    void onError() override
    {
        ASSERT(m_state == Streaming);
        error();
        m_bodyStreamBuffer->unregisterObserver();
    }

    // FileReaderLoaderClient functions.
    void didStartLoading() override { }
    void didReceiveData() override { }
    void didFinishLoading() override
    {
        ASSERT(m_state == ReadingBlob);
        write(m_loader->arrayBufferResult());
        close();
    }
    void didFail(FileError::ErrorCode) override
    {
        ASSERT(m_state == ReadingBlob);
        error();
    }

    void write(PassRefPtr<DOMArrayBuffer> buf)
    {
        if (m_drainingStreamBuffer) {
            m_drainingStreamBuffer->write(buf);
        } else {
            ++m_queueCount;
            m_stream->enqueue(buf);
        }
    }
    void close()
    {
        m_state = Closed;
        if (m_drainingStreamBuffer)
            m_drainingStreamBuffer->close();
        else
            m_stream->close();
    }
    void error()
    {
        m_state = Errored;
        if (m_drainingStreamBuffer)
            m_drainingStreamBuffer->error(exception());
        else
            m_stream->error(exception());
    }
    PassRefPtrWillBeRawPtr<DOMException> exception()
    {
        if (m_state != Errored)
            return nullptr;
        if (m_bodyStreamBuffer) {
            ASSERT(m_bodyStreamBuffer->exception());
            return m_bodyStreamBuffer->exception();
        }
        return DOMException::create(NetworkError, "network error");
    }

    Member<Body> m_body;
    // Set when the data container of the Body is a BodyStreamBuffer.
    Member<BodyStreamBuffer> m_bodyStreamBuffer;
    // Set when the data container of the Body is a BlobDataHandle.
    RefPtr<BlobDataHandle> m_blobDataHandle;
    // Used to read the data from BlobDataHandle.
    OwnPtr<FileReaderLoader> m_loader;
    // Created when createDrainingStream is called to drain the data.
    Member<BodyStreamBuffer> m_drainingStreamBuffer;
    Member<ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>> m_stream;
    State m_state;
    // The count of the chunks which were enqueued to the ReadableStream.
    size_t m_queueCount;
};

ScriptPromise Body::readAsync(ScriptState* scriptState, ResponseType type)
{
    if (bodyUsed())
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

    setBodyUsed();
    m_responseType = type;

    ASSERT(!m_resolver);
    m_resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = m_resolver->promise();

    if (m_stream) {
        ASSERT(m_streamSource);
        bool dataLost;
        m_streamSource->createDrainingStream(&dataLost)->readAllAndCreateBlobHandle(contentTypeForBuffer(), new BlobHandleReceiver(this));
    } else if (buffer()) {
        buffer()->readAllAndCreateBlobHandle(contentTypeForBuffer(), new BlobHandleReceiver(this));
    } else {
        readAsyncFromBlob(blobDataHandle());
    }
    return promise;
}

void Body::readAsyncFromBlob(PassRefPtr<BlobDataHandle> handle)
{
    FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsText;
    RefPtr<BlobDataHandle> blobHandle = handle;
    if (!blobHandle)
        blobHandle = BlobDataHandle::create(BlobData::create(), 0);
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
    if (!m_stream) {
        ASSERT(!m_streamSource);
        m_streamSource = new ReadableStreamSource(this);
        m_stream = new ReadableStreamImpl<ReadableStreamChunkTypeTraits<DOMArrayBuffer>>(executionContext(), m_streamSource);
        m_streamSource->startStream(m_stream);
    }
    return m_stream;
}

bool Body::bodyUsed() const
{
    return m_bodyUsed || (m_stream && m_stream->isLocked());
}

void Body::setBodyUsed()
{
    ASSERT(!m_bodyUsed);
    ASSERT(!m_stream || !m_stream->isLocked());
    // Note that technically we can set BodyUsed even when the stream is
    // closed or errored, but getReader doesn't work then.
    if (m_stream && m_stream->stateInternal() != ReadableStream::Closed && m_stream->stateInternal() != ReadableStream::Errored) {
        TrackExceptionState exceptionState;
        m_streamReader = m_stream->getReader(exceptionState);
        ASSERT(!exceptionState.hadException());
    }
    m_bodyUsed = true;
}

bool Body::streamAccessed() const
{
    return m_stream;
}

BodyStreamBuffer* Body::createDrainingStream(bool* dataLost)
{
    ASSERT(m_stream);
    BodyStreamBuffer* newBuffer = m_streamSource->createDrainingStream(dataLost);
    m_stream = nullptr;
    m_streamSource = nullptr;
    return newBuffer;
}

void Body::stop()
{
    // Canceling the load will call didFail which will remove the resolver.
    if (m_loader)
        m_loader->cancel();
}

bool Body::hasPendingActivity() const
{
    if (executionContext()->activeDOMObjectsAreStopped())
        return false;
    if (m_resolver)
        return true;
    if (m_stream && m_stream->hasPendingActivity())
        return true;
    return false;
}

DEFINE_TRACE(Body)
{
    visitor->trace(m_resolver);
    visitor->trace(m_stream);
    visitor->trace(m_streamSource);
    visitor->trace(m_streamReader);
    ActiveDOMObject::trace(visitor);
}

Body::Body(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_bodyUsed(false)
    , m_responseType(ResponseType::ResponseUnknown)
{
}

Body::Body(const Body& copy_from)
    : ActiveDOMObject(copy_from.lifecycleContext())
    , m_bodyUsed(copy_from.bodyUsed())
    , m_responseType(ResponseType::ResponseUnknown)
{
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
}

void Body::didBlobHandleReceiveError(PassRefPtrWillBeRawPtr<DOMException> exception)
{
    if (!m_resolver)
        return;
    m_resolver->reject(exception);
    m_resolver.clear();
}

} // namespace blink
