// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "bindings/core/v8/Dictionary.h"
#include "modules/serviceworkers/Headers.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct RequestInit {
    explicit RequestInit(const Dictionary& options)
    {
        options.get("method", method);
        options.get("headers", headers);
        if (!headers) {
            options.get("headers", headersDictionary);
        }
        options.get("mode", mode);
        options.get("credentials", credentials);
    }

    String method;
    RefPtr<Headers> headers;
    Dictionary headersDictionary;
    String mode;
    String credentials;
};

}

#endif // RequestInit_h
