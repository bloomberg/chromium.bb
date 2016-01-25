// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/RequestInit.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8FormData.h"
#include "bindings/core/v8/V8URLSearchParams.h"
#include "bindings/modules/v8/V8PasswordCredential.h"
#include "core/dom/URLSearchParams.h"
#include "core/fileapi/Blob.h"
#include "core/html/FormData.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "modules/fetch/Headers.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/ReferrerPolicy.h"

namespace blink {

RequestInit::RequestInit(ExecutionContext* context, const Dictionary& options, ExceptionState& exceptionState)
    : areAnyMembersSet(false)
    , isCredentialRequest(false)
{
    areAnyMembersSet |= DictionaryHelper::get(options, "method", method);
    areAnyMembersSet |= DictionaryHelper::get(options, "headers", headers);
    if (!headers) {
        Vector<Vector<String>> headersVector;
        if (DictionaryHelper::get(options, "headers", headersVector, exceptionState)) {
            headers = Headers::create(headersVector, exceptionState);
            areAnyMembersSet = true;
        } else {
            areAnyMembersSet |= DictionaryHelper::get(options, "headers", headersDictionary);
        }
    }
    areAnyMembersSet |= DictionaryHelper::get(options, "mode", mode);
    areAnyMembersSet |= DictionaryHelper::get(options, "credentials", credentials);
    areAnyMembersSet |= DictionaryHelper::get(options, "redirect", redirect);
    AtomicString referrerString;
    bool isReferrerStringSet = DictionaryHelper::get(options, "referrer", referrerString);
    areAnyMembersSet |= isReferrerStringSet;
    areAnyMembersSet |= DictionaryHelper::get(options, "integrity", integrity);

    v8::Local<v8::Value> v8Body;
    bool isBodySet = DictionaryHelper::get(options, "body", v8Body);
    areAnyMembersSet |= isBodySet;

    if (areAnyMembersSet) {
        // A part of the Request constructor algorithm is performed here. See
        // the comments in the Request constructor code for the detail.

        // We need to use "about:client" instead of |clientReferrerString|,
        // because "about:client" => |clientReferrerString| conversion is done
        // in Request::createRequestWithRequestOrString.
        referrer = Referrer("about:client", ReferrerPolicyDefault);
        if (isReferrerStringSet)
            referrer.referrer = referrerString;
    }

    if (!isBodySet || v8Body->IsUndefined() || v8Body->IsNull())
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
        RefPtr<EncodedFormData> formData = V8FormData::toImpl(v8::Local<v8::Object>::Cast(v8Body))->encodeMultiPartFormData();
        // Here we handle formData->boundary() as a C-style string. See
        // FormDataEncoder::generateUniqueBoundaryString.
        contentType = AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + formData->boundary().data();
        body = FetchFormDataConsumerHandle::create(context, formData.release());
    } else if (V8URLSearchParams::hasInstance(v8Body, isolate)) {
        RefPtr<EncodedFormData> formData = V8URLSearchParams::toImpl(v8::Local<v8::Object>::Cast(v8Body))->encodeFormData();
        contentType = AtomicString("application/x-www-form-urlencoded;charset=UTF-8", AtomicString::ConstructFromLiteral);
        body = FetchFormDataConsumerHandle::create(context, formData.release());
    } else if (V8PasswordCredential::hasInstance(v8Body, isolate)) {
        // See https://w3c.github.io/webappsec-credential-management/#monkey-patching-fetch-4
        // and https://w3c.github.io/webappsec-credential-management/#monkey-patching-fetch-3
        isCredentialRequest = true;
        PasswordCredential* credential = V8PasswordCredential::toImpl(v8::Local<v8::Object>::Cast(v8Body));

        RefPtr<EncodedFormData> encodedData = credential->encodeFormData(contentType);
        body = FetchFormDataConsumerHandle::create(context, encodedData.release());
    } else if (v8Body->IsString()) {
        contentType = "text/plain;charset=UTF-8";
        body = FetchFormDataConsumerHandle::create(toUSVString(isolate, v8Body, exceptionState));
    }
}

} // namespace blink
