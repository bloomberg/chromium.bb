// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/modules/v8/byte_string_sequence_sequence_or_byte_string_byte_string_record.h"
#include "modules/fetch/Headers.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class BytesConsumer;
class Dictionary;
class ExecutionContext;
class ExceptionState;

// FIXME: Use IDL dictionary instead of this class.
class RequestInit {
  STACK_ALLOCATED();

 public:
  RequestInit(ExecutionContext*, const Dictionary&, ExceptionState&);

  const String& Method() const { return method_; }
  const HeadersInit& GetHeaders() const { return headers_; }
  const String& ContentType() const { return content_type_; }
  BytesConsumer* GetBody() { return body_; }
  const Referrer& GetReferrer() const { return referrer_; }
  const String& Mode() const { return mode_; }
  const String& Credentials() const { return credentials_; }
  const String& CacheMode() const { return cache_; }
  const String& Redirect() const { return redirect_; }
  const String& Integrity() const { return integrity_; }
  RefPtr<EncodedFormData> AttachedCredential() { return attached_credential_; }
  bool AreAnyMembersSet() const { return are_any_members_set_; }

 private:
  // These are defined here to avoid JUMBO ambiguity.
  class GetterHelper;
  struct IDLPassThrough;
  friend struct NativeValueTraits<IDLPassThrough>;
  friend struct NativeValueTraitsBase<IDLPassThrough>;

  void SetUpReferrer(const WTF::Optional<String>& referrer_string,
                     const WTF::Optional<String>& referrer_policy_string,
                     ExceptionState&);
  void SetUpCredentials(ExecutionContext*,
                        v8::Isolate*,
                        v8::Local<v8::Value> v8_credentials,
                        ExceptionState&);
  void SetUpBody(ExecutionContext*,
                 v8::Isolate*,
                 v8::Local<v8::Value> v8_body,
                 ExceptionState&);

  String method_;
  HeadersInit headers_;
  String content_type_;
  Member<BytesConsumer> body_;
  Referrer referrer_;
  String mode_;
  String credentials_;
  String cache_;
  String redirect_;
  String integrity_;
  RefPtr<EncodedFormData> attached_credential_;
  // True if any members in RequestInit are set and hence the referrer member
  // should be used in the Request constructor.
  bool are_any_members_set_ = false;
};

}  // namespace blink

#endif  // RequestInit_h
