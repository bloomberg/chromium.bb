// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeadersForEachCallback_h
#define HeadersForEachCallback_h

#include "bindings/v8/ScriptValue.h"

namespace WebCore {

class Headers;

class HeadersForEachCallback {
public:
    virtual ~HeadersForEachCallback() { }
    virtual bool handleItem(ScriptValue thisValue, const String& value, const String& key, Headers*) = 0;
    virtual bool handleItem(const String& value, const String& key, Headers*) = 0;
};

} // namespace WebCore

#endif // HeadersForEachCallback_h
