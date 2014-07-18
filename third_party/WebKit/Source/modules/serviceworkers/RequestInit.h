// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "bindings/core/v8/Dictionary.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class RequestInit {
    STACK_ALLOCATED();
public:
    explicit RequestInit(const Dictionary& options)
    {
        DictionaryHelper::get(options, "method", method);
        DictionaryHelper::get(options, "headers", headers);
        if (!headers) {
            DictionaryHelper::get(options, "headers", headersDictionary);
        }
        DictionaryHelper::get(options, "mode", mode);
        DictionaryHelper::get(options, "credentials", credentials);
    }

    String method;
    RefPtrWillBeMember<Headers> headers;
    Dictionary headersDictionary;
    String mode;
    String credentials;
};

}

#endif // RequestInit_h
