// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/RequestInit.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
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

struct RequestInit::IDLPassThrough
    : public IDLBaseHelper<v8::Local<v8::Value>> {};

template <>
struct NativeValueTraits<RequestInit::IDLPassThrough>
    : public NativeValueTraitsBase<RequestInit::IDLPassThrough> {
  static v8::Local<v8::Value> NativeValue(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value,
                                          ExceptionState& exception_state) {
    DCHECK(!value.IsEmpty());
    return value;
  }
};

class RequestInit::GetterHelper {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(GetterHelper);

 public:
  // |this| object must not outlive |src| and |exception_state|.
  GetterHelper(const Dictionary& src, ExceptionState& exception_state)
      : src_(src), exception_state_(exception_state) {}

  template <typename IDLType>
  WTF::Optional<typename IDLType::ImplType> Get(const StringView& key) {
    auto r = src_.Get<IDLType>(key, exception_state_);
    are_any_members_set_ = are_any_members_set_ || r.has_value();
    return r;
  }

  bool AreAnyMembersSet() const { return are_any_members_set_; }

 private:
  const Dictionary& src_;
  ExceptionState& exception_state_;
  bool are_any_members_set_ = false;
};

RequestInit::RequestInit(ExecutionContext* context,
                         const Dictionary& options,
                         ExceptionState& exception_state) {
  GetterHelper h(options, exception_state);

  method_ = h.Get<IDLByteString>("method").value_or(String());
  if (exception_state.HadException())
    return;

  auto v8_headers = h.Get<IDLPassThrough>("headers");
  if (exception_state.HadException())
    return;

  mode_ = h.Get<IDLUSVString>("mode").value_or(String());
  if (exception_state.HadException())
    return;

  if (RuntimeEnabledFeatures::FetchRequestCacheEnabled()) {
    cache_ = h.Get<IDLUSVString>("cache").value_or(String());
    if (exception_state.HadException())
      return;
  }

  redirect_ = h.Get<IDLUSVString>("redirect").value_or(String());
  if (exception_state.HadException())
    return;

  auto referrer_string = h.Get<IDLUSVString>("referrer");
  if (exception_state.HadException())
    return;

  auto referrer_policy_string = h.Get<IDLUSVString>("referrerPolicy");
  if (exception_state.HadException())
    return;

  integrity_ = h.Get<IDLString>("integrity").value_or(String());
  if (exception_state.HadException())
    return;

  auto v8_body = h.Get<IDLPassThrough>("body");
  if (exception_state.HadException())
    return;

  auto v8_credentials = h.Get<IDLPassThrough>("credentials");
  if (exception_state.HadException())
    return;

  are_any_members_set_ = h.AreAnyMembersSet();

  SetUpReferrer(referrer_string, referrer_policy_string, exception_state);
  if (exception_state.HadException())
    return;

  v8::Isolate* isolate = ToIsolate(context);

  if (v8_headers.has_value()) {
    V8ByteStringSequenceSequenceOrByteStringByteStringRecord::ToImpl(
        isolate, *v8_headers, headers_, UnionTypeConversionMode::kNotNullable,
        exception_state);
    if (exception_state.HadException())
      return;
  }

  if (v8_credentials.has_value()) {
    SetUpCredentials(context, isolate, *v8_credentials, exception_state);
    if (exception_state.HadException())
      return;
  }

  if (v8_body.has_value()) {
    SetUpBody(context, isolate, *v8_body, exception_state);
    if (exception_state.HadException())
      return;
  }
}

void RequestInit::SetUpReferrer(
    const WTF::Optional<String>& referrer_string,
    const WTF::Optional<String>& referrer_policy_string,
    ExceptionState& exception_state) {
  if (!are_any_members_set_)
    return;

  // A part of the Request constructor algorithm is performed here. See
  // the comments in the Request constructor code for the detail.

  // We need to use "about:client" instead of |clientReferrerString|,
  // because "about:client" => |clientReferrerString| conversion is done
  // in Request::createRequestWithRequestOrString.
  referrer_ = Referrer("about:client", kReferrerPolicyDefault);
  if (referrer_string.has_value())
    referrer_.referrer = AtomicString(*referrer_string);
  if (referrer_policy_string.has_value()) {
    if (*referrer_policy_string == "") {
      referrer_.referrer_policy = kReferrerPolicyDefault;
    } else if (*referrer_policy_string == "no-referrer") {
      referrer_.referrer_policy = kReferrerPolicyNever;
    } else if (*referrer_policy_string == "no-referrer-when-downgrade") {
      referrer_.referrer_policy = kReferrerPolicyNoReferrerWhenDowngrade;
    } else if (*referrer_policy_string == "origin") {
      referrer_.referrer_policy = kReferrerPolicyOrigin;
    } else if (*referrer_policy_string == "origin-when-cross-origin") {
      referrer_.referrer_policy = kReferrerPolicyOriginWhenCrossOrigin;
    } else if (*referrer_policy_string == "same-origin") {
      referrer_.referrer_policy = kReferrerPolicySameOrigin;
    } else if (*referrer_policy_string == "strict-origin") {
      referrer_.referrer_policy = kReferrerPolicyStrictOrigin;
    } else if (*referrer_policy_string == "unsafe-url") {
      referrer_.referrer_policy = kReferrerPolicyAlways;
    } else if (*referrer_policy_string == "strict-origin-when-cross-origin") {
      referrer_.referrer_policy =
          kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      exception_state.ThrowTypeError("Invalid referrer policy");
      return;
    }
  }
}

void RequestInit::SetUpCredentials(ExecutionContext* context,
                                   v8::Isolate* isolate,
                                   v8::Local<v8::Value> v8_credentials,
                                   ExceptionState& exception_state) {
  if (V8PasswordCredential::hasInstance(v8_credentials, isolate)) {
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
        V8PasswordCredential::ToImpl(v8_credentials.As<v8::Object>());
    attached_credential_ = credential->EncodeFormData(content_type_);
    credentials_ = "password";
  } else if (v8_credentials->IsString()) {
    credentials_ = ToUSVString(isolate, v8_credentials, exception_state);
    if (exception_state.HadException())
      return;
  }
}

void RequestInit::SetUpBody(ExecutionContext* context,
                            v8::Isolate* isolate,
                            v8::Local<v8::Value> v8_body,
                            ExceptionState& exception_state) {
  if (attached_credential_ || v8_body->IsNull())
    return;

  if (v8_body->IsArrayBuffer()) {
    body_ = new FormDataBytesConsumer(
        V8ArrayBuffer::ToImpl(v8_body.As<v8::Object>()));
  } else if (v8_body->IsArrayBufferView()) {
    body_ = new FormDataBytesConsumer(
        V8ArrayBufferView::ToImpl(v8_body.As<v8::Object>()));
  } else if (V8Blob::hasInstance(v8_body, isolate)) {
    scoped_refptr<BlobDataHandle> blob_data_handle =
        V8Blob::ToImpl(v8_body.As<v8::Object>())->GetBlobDataHandle();
    content_type_ = blob_data_handle->GetType();
    body_ = new BlobBytesConsumer(context, std::move(blob_data_handle));
  } else if (V8FormData::hasInstance(v8_body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
        V8FormData::ToImpl(v8_body.As<v8::Object>())->EncodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    content_type_ = AtomicString("multipart/form-data; boundary=") +
                    form_data->Boundary().data();
    body_ = new FormDataBytesConsumer(context, std::move(form_data));
  } else if (V8URLSearchParams::hasInstance(v8_body, isolate)) {
    scoped_refptr<EncodedFormData> form_data =
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
