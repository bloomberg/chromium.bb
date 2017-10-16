// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/RequestInit.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8FormData.h"
#include "bindings/core/v8/V8URLSearchParams.h"
#include "bindings/modules/v8/V8PasswordCredential.h"
#include "core/fileapi/Blob.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/html/forms/FormData.h"
#include "core/url/URLSearchParams.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "modules/fetch/Headers.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "platform/runtime_enabled_features.h"
#include "platform/weborigin/ReferrerPolicy.h"

namespace blink {

// TODO(yiyix): Verify if any DictionaryHelper::get should be replaced with
// DictionaryHelper::getWithUndefinedCheck.
RequestInit::RequestInit(ExecutionContext* context,
                         const Dictionary& options,
                         ExceptionState& exception_state) {
  are_any_members_set_ |= DictionaryHelper::Get(options, "method", method_);

  // From https://github.com/whatwg/fetch/issues/479:
  // - undefined is the same as "this member has not been passed".
  // - {} means "the list of headers is empty", so the member has been set.
  // - null is an invalid value for both sequences and records, but it is not
  //   the same as undefined: a value has been set, even if invalid, and will
  //   throw a TypeError later when it gets converted to a HeadersInit object.
  v8::Local<v8::Value> v8_headers;
  bool is_header_set =
      options.Get("headers", v8_headers) && !v8_headers->IsUndefined();
  are_any_members_set_ |= is_header_set;

  are_any_members_set_ |= DictionaryHelper::Get(options, "mode", mode_);
  if (RuntimeEnabledFeatures::FetchRequestCacheEnabled())
    are_any_members_set_ |= DictionaryHelper::Get(options, "cache", cache_);

  are_any_members_set_ |= DictionaryHelper::Get(options, "redirect", redirect_);
  AtomicString referrer_string;
  bool is_referrer_string_set = DictionaryHelper::GetWithUndefinedCheck(
      options, "referrer", referrer_string);
  are_any_members_set_ |= is_referrer_string_set;
  are_any_members_set_ |=
      DictionaryHelper::Get(options, "integrity", integrity_);
  AtomicString referrer_policy_string;
  bool is_referrer_policy_set =
      DictionaryHelper::Get(options, "referrerPolicy", referrer_policy_string);
  are_any_members_set_ |= is_referrer_policy_set;

  v8::Local<v8::Value> v8_body;
  bool is_body_set = options.Get("body", v8_body);
  are_any_members_set_ |= is_body_set;

  v8::Local<v8::Value> v8_credential;
  bool is_credential_set = options.Get("credentials", v8_credential);
  are_any_members_set_ |= is_credential_set;

  if (are_any_members_set_) {
    // A part of the Request constructor algorithm is performed here. See
    // the comments in the Request constructor code for the detail.

    // We need to use "about:client" instead of |clientReferrerString|,
    // because "about:client" => |clientReferrerString| conversion is done
    // in Request::createRequestWithRequestOrString.
    referrer_ = Referrer("about:client", kReferrerPolicyDefault);
    if (is_referrer_string_set)
      referrer_.referrer = referrer_string;
    if (is_referrer_policy_set) {
      if (referrer_policy_string == "") {
        referrer_.referrer_policy = kReferrerPolicyDefault;
      } else if (referrer_policy_string == "no-referrer") {
        referrer_.referrer_policy = kReferrerPolicyNever;
      } else if (referrer_policy_string == "no-referrer-when-downgrade") {
        referrer_.referrer_policy = kReferrerPolicyNoReferrerWhenDowngrade;
      } else if (referrer_policy_string == "origin") {
        referrer_.referrer_policy = kReferrerPolicyOrigin;
      } else if (referrer_policy_string == "origin-when-cross-origin") {
        referrer_.referrer_policy = kReferrerPolicyOriginWhenCrossOrigin;
      } else if (referrer_policy_string == "same-origin") {
        referrer_.referrer_policy = kReferrerPolicySameOrigin;
      } else if (referrer_policy_string == "strict-origin") {
        referrer_.referrer_policy = kReferrerPolicyStrictOrigin;
      } else if (referrer_policy_string == "unsafe-url") {
        referrer_.referrer_policy = kReferrerPolicyAlways;
      } else if (referrer_policy_string == "strict-origin-when-cross-origin") {
        referrer_.referrer_policy =
            kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
      } else {
        exception_state.ThrowTypeError("Invalid referrer policy");
        return;
      }
    }
  }

  v8::Isolate* isolate = ToIsolate(context);

  if (is_header_set) {
    V8ByteStringSequenceSequenceOrByteStringByteStringRecord::ToImpl(
        isolate, v8_headers, headers_, UnionTypeConversionMode::kNotNullable,
        exception_state);
    if (exception_state.HadException())
      return;
  }

  if (is_credential_set) {
    if (V8PasswordCredential::hasInstance(v8_credential, isolate)) {
      Deprecation::CountDeprecation(context,
                                    WebFeature::kCredentialManagerCustomFetch);
      // TODO(mkwst): According to the spec, we'd serialize this once we touch
      // the network. We're serializing it here, ahead of time, because lifetime
      // issues around ResourceRequest make it pretty difficult to pass a
      // PasswordCredential around at the platform level, and the hop between
      // the browser and renderer processes to deal with service workers is
      // equally painful. There should be no developer-visible difference in
      // behavior with this option, except that the `Content-Type` header will
      // be set early. That seems reasonable.
      PasswordCredential* credential =
          V8PasswordCredential::ToImpl(v8_credential.As<v8::Object>());
      attached_credential_ = credential->EncodeFormData(content_type_);
      credentials_ = "password";
    } else if (v8_credential->IsString()) {
      credentials_ = ToUSVString(isolate, v8_credential, exception_state);
    }
  }

  if (attached_credential_.get() || !is_body_set || v8_body->IsUndefined() ||
      v8_body->IsNull())
    return;

  if (v8_body->IsArrayBuffer()) {
    body_ = new FormDataBytesConsumer(
        V8ArrayBuffer::ToImpl(v8_body.As<v8::Object>()));
  } else if (v8_body->IsArrayBufferView()) {
    body_ = new FormDataBytesConsumer(
        V8ArrayBufferView::ToImpl(v8_body.As<v8::Object>()));
  } else if (V8Blob::hasInstance(v8_body, isolate)) {
    RefPtr<BlobDataHandle> blob_data_handle =
        V8Blob::ToImpl(v8_body.As<v8::Object>())->GetBlobDataHandle();
    content_type_ = blob_data_handle->GetType();
    body_ = new BlobBytesConsumer(context, std::move(blob_data_handle));
  } else if (V8FormData::hasInstance(v8_body, isolate)) {
    RefPtr<EncodedFormData> form_data =
        V8FormData::ToImpl(v8_body.As<v8::Object>())->EncodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    content_type_ = AtomicString("multipart/form-data; boundary=") +
                    form_data->Boundary().data();
    body_ = new FormDataBytesConsumer(context, std::move(form_data));
  } else if (V8URLSearchParams::hasInstance(v8_body, isolate)) {
    RefPtr<EncodedFormData> form_data =
        V8URLSearchParams::ToImpl(v8_body.As<v8::Object>())
            ->ToEncodedFormData();
    content_type_ =
        AtomicString("application/x-www-form-urlencoded;charset=UTF-8");
    body_ = new FormDataBytesConsumer(context, std::move(form_data));
  } else if (v8_body->IsString()) {
    content_type_ = "text/plain;charset=UTF-8";
    body_ = new FormDataBytesConsumer(
        ToUSVString(isolate, v8_body, exception_state));
  }
}

}  // namespace blink
