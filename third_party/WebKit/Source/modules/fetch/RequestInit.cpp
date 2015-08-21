// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/RequestInit.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8FormData.h"
#include "core/fileapi/Blob.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "modules/fetch/Headers.h"
#include "platform/blob/BlobData.h"
#include "platform/network/FormData.h"

namespace blink {

RequestInit::RequestInit(ExecutionContext* context, const Dictionary& options, ExceptionState& exceptionState)
{
    DictionaryHelper::get(options, "method", method);
    DictionaryHelper::get(options, "headers", headers);
    if (!headers) {
        Vector<Vector<String>> headersVector;
        if (DictionaryHelper::get(options, "headers", headersVector, exceptionState))
            headers = Headers::create(headersVector, exceptionState);
        else
            DictionaryHelper::get(options, "headers", headersDictionary);
    }
    DictionaryHelper::get(options, "mode", mode);
    DictionaryHelper::get(options, "credentials", credentials);
    DictionaryHelper::get(options, "redirect", redirect);
    DictionaryHelper::get(options, "integrity", integrity);

    v8::Local<v8::Value> v8Body;
    if (!DictionaryHelper::get(options, "body", v8Body) || v8Body->IsUndefined() || v8Body->IsNull())
        return;
    v8::Isolate* isolate = toIsolate(context);
    if (v8Body->IsArrayBuffer()) {
        body = FetchFormDataConsumerHandle::create(V8ArrayBuffer::toImpl(v8::Local<v8::Object>::Cast(v8Body)));
    } else if (v8Body->IsArrayBufferView()) {
        body = FetchFormDataConsumerHandle::create(V8ArrayBufferView::toImpl(v8::Local<v8::Object>::Cast(v8Body)));
    } else if (V8Blob::hasInstance(v8Body, isolate)) {
        RefPtr<BlobDataHandle> blobDataHandle = V8Blob::toImpl(v8::Local<v8::Object>::Cast(v8Body))->blobDataHandle();
        contentType = blobDataHandle->type();
        body = FetchBlobDataConsumerHandle::create(context, blobDataHandle.release());
    } else if (V8FormData::hasInstance(v8Body, isolate)) {
        RefPtr<FormData> formData = V8FormData::toImpl(v8::Local<v8::Object>::Cast(v8Body))->createMultiPartFormData();
        // Here we handle formData->boundary() as a C-style string. See
        // FormDataBuilder::generateUniqueBoundaryString.
        contentType = AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + formData->boundary().data();
        body = FetchFormDataConsumerHandle::create(context, formData.release());
    } else if (v8Body->IsString()) {
        contentType = "text/plain;charset=UTF-8";
        body = FetchFormDataConsumerHandle::create(toUSVString(isolate, v8Body, exceptionState));
    }
}

}
