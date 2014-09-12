// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Body.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"

namespace blink {

ScriptPromise Body::readAsync(ScriptState* scriptState, ResponseType type)
{
    if (m_bodyUsed)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError("Already read", scriptState->isolate()));

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

    FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsText;
    RefPtr<BlobDataHandle> blobHandle = blobDataHandle();
    if (!blobHandle.get()) {
        blobHandle = BlobDataHandle::create(BlobData::create(), 0);
    }
    switch (type) {
    case ResponseAsArrayBuffer:
        readType = FileReaderLoader::ReadAsArrayBuffer;
        break;
    case ResponseAsBlob:
        if (blobHandle->size() != kuint64max) {
            // If the size of |blobHandle| is set correctly, creates Blob from
            // it.
            m_resolver->resolve(Blob::create(blobHandle));
            m_resolver.clear();
            return promise;
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
    m_loader->start(executionContext, blobHandle);

    return promise;
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

bool Body::bodyUsed() const
{
    return m_bodyUsed;
}

void Body::setBodyUsed()
{
    m_bodyUsed = true;
}

void Body::stop()
{
    // Canceling the load will call didFail which will remove the resolver.
    if (m_resolver)
        m_loader->cancel();
}

bool Body::hasPendingActivity() const
{
    return m_resolver;
}

Body::Body(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_bodyUsed(false)
{
}

void Body::resolveJSON()
{
    ASSERT(m_responseType == ResponseAsJSON);
    ScriptState::Scope scope(m_resolver->scriptState());
    v8::Isolate* isolate = m_resolver->scriptState()->isolate();
    v8::Local<v8::String> inputString = v8String(isolate, m_loader->stringResult());
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
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;

    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        m_resolver->resolve(m_loader->arrayBufferResult());
        break;
    case ResponseAsBlob: {
        ASSERT(blobDataHandle()->size() == kuint64max);
        OwnPtr<BlobData> blobData = BlobData::create();
        RefPtr<ArrayBuffer> buffer = m_loader->arrayBufferResult();
        blobData->appendArrayBuffer(buffer.get());
        const size_t length = blobData->length();
        m_resolver->resolve(Blob::create(BlobDataHandle::create(blobData.release(), length)));
        break;
    }
    case ResponseAsFormData:
        ASSERT_NOT_REACHED();
        break;
    case ResponseAsJSON:
        resolveJSON();
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
    ASSERT(m_resolver);
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;

    m_resolver->resolve("");
    m_resolver.clear();
}

} // namespace blink
