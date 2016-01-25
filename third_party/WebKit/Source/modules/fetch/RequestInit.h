// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "bindings/core/v8/Dictionary.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class FetchDataConsumerHandle;
class Headers;

// FIXME: Use IDL dictionary instead of this class.
class RequestInit {
    STACK_ALLOCATED();
public:
    explicit RequestInit(ExecutionContext*, const Dictionary&, ExceptionState&);

    String method;
    Member<Headers> headers;
    Dictionary headersDictionary;
    String contentType;
    OwnPtr<FetchDataConsumerHandle> body;
    Referrer referrer;
    String mode;
    String credentials;
    String redirect;
    String integrity;
    // True if any members in RequestInit are set and hence the referrer member
    // should be used in the Request constructor.
    bool areAnyMembersSet;
    // True if the RequestInit |body| was created from a 'PasswordCredential' object.
    bool isCredentialRequest;
};

} // namespace blink

#endif // RequestInit_h
