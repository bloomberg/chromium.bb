// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class BytesConsumer;
class Dictionary;
class ExecutionContext;
class ExceptionState;
class Headers;

// FIXME: Use IDL dictionary instead of this class.
class RequestInit {
  STACK_ALLOCATED();

 public:
  explicit RequestInit(ExecutionContext*, const Dictionary&, ExceptionState&);

  String method;
  Member<Headers> headers;
  String content_type;
  Member<BytesConsumer> body;
  Referrer referrer;
  String mode;
  String credentials;
  String cache;
  String redirect;
  String integrity;
  RefPtr<EncodedFormData> attached_credential;
  // True if any members in RequestInit are set and hence the referrer member
  // should be used in the Request constructor.
  bool are_any_members_set;
};

}  // namespace blink

#endif  // RequestInit_h
