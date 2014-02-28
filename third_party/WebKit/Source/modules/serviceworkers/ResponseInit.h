// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResponseInit_h
#define ResponseInit_h

#include "bindings/v8/Dictionary.h"

namespace WebCore {

struct ResponseInit {
    explicit ResponseInit(const Dictionary& options)
        : statusCode(200)
        , statusText("OK")
    {
        options.get("statusCode", statusCode);
        options.get("statusTest", statusText);
        options.get("method", method);
        options.get("headers", headers);
    }

    unsigned short statusCode;
    String statusText;
    String method;
    Dictionary headers;
};

}

#endif // ResponseInit_h
