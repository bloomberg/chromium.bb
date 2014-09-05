// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/FetchBodyStream.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "modules/serviceworkers/ResponseInit.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace blink {

FetchBodyStream* FetchBodyStream::create(ExecutionContext* context, PassRefPtr<BlobDataHandle> blobDataHandle)
{
    FetchBodyStream* fetchBodyStream = new FetchBodyStream(context, blobDataHandle);
    fetchBodyStream->suspendIfNeeded();
    return fetchBodyStream;
}

ScriptPromise FetchBodyStream::readAsync(ScriptState* scriptState, ResponseType type)
{
    if (m_hasRead)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError("Already read", scriptState->isolate()));

    m_hasRead = true;
    m_responseType = type;

    ASSERT(!m_resolver);
    m_resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = m_resolver->promise();

    FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsText;

    switch (type) {
    case ResponseAsArrayBuffer:
        readType = FileReaderLoader::ReadAsArrayBuffer;
        break;
    case ResponseAsBlob:
        if (m_blobDataHandle->size() != kuint64max) {
            // If the size of |m_blobDataHandle| is set correctly, creates Blob from it.
            m_resolver->resolve(Blob::create(m_blobDataHandle));
            m_resolver.clear();
            return promise;
        }
        // If the size is not set, read as ArrayBuffer and create a new blob to get the size.
        // FIXME: This workaround is not good for performance.
        // When we will stop using Blob as a base system of FetchBodyStream to support stream, this problem should be solved.
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
    m_loader->start(scriptState->executionContext(), m_blobDataHandle);

    return promise;
}

ScriptPromise FetchBodyStream::asArrayBuffer(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsArrayBuffer);
}

ScriptPromise FetchBodyStream::asBlob(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsBlob);
}

ScriptPromise FetchBodyStream::asFormData(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsFormData);
}

ScriptPromise FetchBodyStream::asJSON(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsJSON);
}

ScriptPromise FetchBodyStream::asText(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsText);
}

void FetchBodyStream::stop()
{
    // Canceling the load will call didFail which will remove the resolver.
    if (m_resolver)
        m_loader->cancel();
}

bool FetchBodyStream::hasPendingActivity() const
{
    return m_resolver;
}

FetchBodyStream::FetchBodyStream(ExecutionContext* context, PassRefPtr<BlobDataHandle> blobDataHandle)
    : ActiveDOMObject(context)
    , m_blobDataHandle(blobDataHandle)
    , m_hasRead(false)
{
    ScriptWrappable::init(this);
    if (!m_blobDataHandle) {
        m_blobDataHandle = BlobDataHandle::create(BlobData::create(), 0);
    }
}

void FetchBodyStream::resolveJSON()
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
void FetchBodyStream::didStartLoading() { }
void FetchBodyStream::didReceiveData() { }
void FetchBodyStream::didFinishLoading()
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;

    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        m_resolver->resolve(m_loader->arrayBufferResult());
        break;
    case ResponseAsBlob: {
        ASSERT(m_blobDataHandle->size() == kuint64max);
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

void FetchBodyStream::didFail(FileError::ErrorCode code)
{
    ASSERT(m_resolver);
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;

    m_resolver->resolve("");
    m_resolver.clear();
}

} // namespace blink
