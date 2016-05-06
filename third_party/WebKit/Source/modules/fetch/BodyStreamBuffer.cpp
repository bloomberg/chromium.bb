// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BodyStreamBuffer.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStreamController.h"
#include "core/streams/ReadableStreamOperations.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"

namespace blink {

class BodyStreamBuffer::LoaderClient final : public GarbageCollectedFinalized<LoaderClient>, public ActiveDOMObject, public FetchDataLoader::Client {
    WTF_MAKE_NONCOPYABLE(LoaderClient);
    USING_GARBAGE_COLLECTED_MIXIN(LoaderClient);
public:
    LoaderClient(ExecutionContext* executionContext, BodyStreamBuffer* buffer, FetchDataLoader::Client* client)
        : ActiveDOMObject(executionContext)
        , m_buffer(buffer)
        , m_client(client)
    {
        suspendIfNeeded();
    }

    void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedBlobHandle(blobDataHandle);
    }

    void didFetchDataLoadedArrayBuffer(DOMArrayBuffer* arrayBuffer) override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedArrayBuffer(arrayBuffer);
    }

    void didFetchDataLoadedString(const String& string) override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedString(string);
    }

    void didFetchDataLoadedStream() override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedStream();
    }

    void didFetchDataLoadFailed() override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadFailed();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_buffer);
        visitor->trace(m_client);
        ActiveDOMObject::trace(visitor);
        FetchDataLoader::Client::trace(visitor);
    }

private:
    void stop() override
    {
        m_buffer->stopLoading();
    }

    Member<BodyStreamBuffer> m_buffer;
    Member<FetchDataLoader::Client> m_client;
};

BodyStreamBuffer::BodyStreamBuffer(ScriptState* scriptState, PassOwnPtr<FetchDataConsumerHandle> handle)
    : UnderlyingSourceBase(scriptState)
    , m_scriptState(scriptState)
    , m_handle(std::move(handle))
    , m_reader(m_handle->obtainReader(this))
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(scriptState);
        v8::Local<v8::Value> bodyValue = toV8(this, scriptState);
        ASSERT(!bodyValue.IsEmpty());
        ASSERT(bodyValue->IsObject());
        v8::Local<v8::Object> body = bodyValue.As<v8::Object>();

        ScriptValue readableStream = ReadableStreamOperations::createReadableStream(
            scriptState, this, ReadableStreamOperations::createCountQueuingStrategy(scriptState, 0));
        V8HiddenValue::setHiddenValue(scriptState, body, V8HiddenValue::internalBodyStream(scriptState->isolate()), readableStream.v8Value());
    } else {
        m_stream = new ReadableByteStream(this, new ReadableByteStream::StrictStrategy);
        m_stream->didSourceStart();
    }
}

ScriptValue BodyStreamBuffer::stream()
{
    ScriptState::Scope scope(m_scriptState.get());
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        v8::Local<v8::Value> bodyValue = toV8(this, m_scriptState.get());
        ASSERT(!bodyValue.IsEmpty());
        ASSERT(bodyValue->IsObject());
        v8::Local<v8::Object> body = bodyValue.As<v8::Object>();
        return ScriptValue(m_scriptState.get(), V8HiddenValue::getHiddenValue(m_scriptState.get(), body, V8HiddenValue::internalBodyStream(m_scriptState->isolate())));
    }
    return ScriptValue(m_scriptState.get(), toV8(m_stream, m_scriptState.get()));
}

PassRefPtr<BlobDataHandle> BodyStreamBuffer::drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy policy)
{
    ASSERT(!isStreamLocked());
    ASSERT(!isStreamDisturbed());
    if (isStreamClosed() || isStreamErrored())
        return nullptr;

    RefPtr<BlobDataHandle> blobDataHandle = m_reader->drainAsBlobDataHandle(policy);
    if (blobDataHandle) {
        closeAndLockAndDisturb();
        return blobDataHandle.release();
    }
    return nullptr;
}

PassRefPtr<EncodedFormData> BodyStreamBuffer::drainAsFormData()
{
    ASSERT(!isStreamLocked());
    ASSERT(!isStreamDisturbed());
    if (isStreamClosed() || isStreamErrored())
        return nullptr;

    RefPtr<EncodedFormData> formData = m_reader->drainAsFormData();
    if (formData) {
        closeAndLockAndDisturb();
        return formData.release();
    }
    return nullptr;
}

PassOwnPtr<FetchDataConsumerHandle> BodyStreamBuffer::releaseHandle()
{
    ASSERT(!isStreamLocked());
    ASSERT(!isStreamDisturbed());
    // We need to call these before calling closeAndLockAndDisturb.
    const bool isClosed = isStreamClosed();
    const bool isErrored = isStreamErrored();
    OwnPtr<FetchDataConsumerHandle> handle = m_handle.release();

    closeAndLockAndDisturb();

    if (isClosed) {
        // Note that the stream cannot be "draining", because it doesn't have
        // the internal buffer.
        return createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle());
    }
    if (isErrored)
        return createFetchDataConsumerHandleFromWebHandle(createUnexpectedErrorDataConsumerHandle());

    ASSERT(handle);
    return handle.release();
}

void BodyStreamBuffer::startLoading(FetchDataLoader* loader, FetchDataLoader::Client* client)
{
    ASSERT(!m_loader);
    ASSERT(m_scriptState->contextIsValid());
    OwnPtr<FetchDataConsumerHandle> handle = releaseHandle();
    m_loader = loader;
    loader->start(handle.get(), new LoaderClient(m_scriptState->getExecutionContext(), this, client));
}

void BodyStreamBuffer::pullSource()
{
    ASSERT(!m_streamNeedsMore);
    m_streamNeedsMore = true;
    processData();
}

ScriptPromise BodyStreamBuffer::cancelSource(ScriptState* scriptState, ScriptValue)
{
    ASSERT(scriptState == m_scriptState.get());
    close();
    return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise BodyStreamBuffer::pull(ScriptState* scriptState)
{
    ASSERT(!m_streamNeedsMore);
    ASSERT(scriptState == m_scriptState.get());
    m_streamNeedsMore = true;
    processData();
    return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise BodyStreamBuffer::cancel(ScriptState* scriptState, ScriptValue reason)
{
    ASSERT(scriptState == m_scriptState.get());
    close();
    return ScriptPromise::castUndefined(scriptState);
}

void BodyStreamBuffer::didGetReadable()
{
    if (!m_reader)
        return;

    if (!m_streamNeedsMore) {
        // Perform zero-length read to call close()/error() early.
        size_t readSize;
        WebDataConsumerHandle::Result result = m_reader->read(nullptr, 0, WebDataConsumerHandle::FlagNone, &readSize);
        switch (result) {
        case WebDataConsumerHandle::Ok:
        case WebDataConsumerHandle::ShouldWait:
            return;
        case WebDataConsumerHandle::Done:
            close();
            return;
        case WebDataConsumerHandle::Busy:
        case WebDataConsumerHandle::ResourceExhausted:
        case WebDataConsumerHandle::UnexpectedError:
            error();
            return;
        }
        return;
    }
    processData();
}

bool BodyStreamBuffer::hasPendingActivity() const
{
    if (m_loader)
        return true;
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled())
        return UnderlyingSourceBase::hasPendingActivity();

    return m_stream->stateInternal() == ReadableStream::Readable && m_stream->isLocked();
}

void BodyStreamBuffer::stop()
{
    m_reader = nullptr;
    m_handle = nullptr;
    UnderlyingSourceBase::stop();
}

bool BodyStreamBuffer::isStreamReadable()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(m_scriptState.get());
        return ReadableStreamOperations::isReadable(m_scriptState.get(), stream());
    }
    return m_stream->stateInternal() == ReadableStream::Readable;
}

bool BodyStreamBuffer::isStreamClosed()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(m_scriptState.get());
        return ReadableStreamOperations::isClosed(m_scriptState.get(), stream());
    }
    return m_stream->stateInternal() == ReadableStream::Closed;
}

bool BodyStreamBuffer::isStreamErrored()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(m_scriptState.get());
        return ReadableStreamOperations::isErrored(m_scriptState.get(), stream());
    }
    return m_stream->stateInternal() == ReadableStream::Errored;
}

bool BodyStreamBuffer::isStreamLocked()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(m_scriptState.get());
        return ReadableStreamOperations::isLocked(m_scriptState.get(), stream());
    }
    return m_stream->isLocked();
}

bool BodyStreamBuffer::isStreamDisturbed()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(m_scriptState.get());
        return ReadableStreamOperations::isDisturbed(m_scriptState.get(), stream());
    }
    return m_stream->isDisturbed();
}

void BodyStreamBuffer::closeAndLockAndDisturb()
{
    if (isStreamReadable()) {
        // Note that the stream cannot be "draining", because it doesn't have
        // the internal buffer.
        close();
    }

    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
        ScriptState::Scope scope(m_scriptState.get());
        NonThrowableExceptionState exceptionState;
        ScriptValue reader = ReadableStreamOperations::getReader(m_scriptState.get(), stream(), exceptionState);
        ReadableStreamOperations::defaultReaderRead(m_scriptState.get(), reader);
    } else {
        NonThrowableExceptionState exceptionState;
        m_stream->getBytesReader(m_scriptState->getExecutionContext(), exceptionState);
        m_stream->setIsDisturbed();
    }
}

void BodyStreamBuffer::close()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled())
        controller()->close();
    else
        m_stream->close();
    m_reader = nullptr;
    m_handle = nullptr;
}

void BodyStreamBuffer::error()
{
    if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled())
        controller()->error(DOMException::create(NetworkError, "network error"));
    else
        m_stream->error(DOMException::create(NetworkError, "network error"));
    m_reader = nullptr;
    m_handle = nullptr;
}

void BodyStreamBuffer::processData()
{
    ASSERT(m_reader);
    while (m_streamNeedsMore) {
        const void* buffer;
        size_t available;
        WebDataConsumerHandle::Result result = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &available);
        switch (result) {
        case WebDataConsumerHandle::Ok: {
            DOMUint8Array* array = DOMUint8Array::create(static_cast<const unsigned char*>(buffer), available);
            if (RuntimeEnabledFeatures::responseBodyWithV8ExtraStreamEnabled()) {
                controller()->enqueue(array);
                m_streamNeedsMore = controller()->desiredSize() > 0;
            } else {
                m_streamNeedsMore = m_stream->enqueue(array);
            }
            m_reader->endRead(available);
            break;
        }
        case WebDataConsumerHandle::Done:
            close();
            return;

        case WebDataConsumerHandle::ShouldWait:
            return;

        case WebDataConsumerHandle::Busy:
        case WebDataConsumerHandle::ResourceExhausted:
        case WebDataConsumerHandle::UnexpectedError:
            error();
            return;
        }
    }
}

void BodyStreamBuffer::endLoading()
{
    ASSERT(m_loader);
    m_loader = nullptr;
}

void BodyStreamBuffer::stopLoading()
{
    if (!m_loader)
        return;
    m_loader->cancel();
    m_loader = nullptr;
}

} // namespace blink
