// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ContextInfo_h
#define V8ContextInfo_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"

#include <v8.h>

namespace blink {

class V8ContextInfo {
public:
    V8ContextInfo(v8::Local<v8::Context> context, bool isDefault, const String16& origin, const String16& humanReadableName, const String16& frameId)
        : context(context)
        , isDefault(isDefault)
        , origin(origin)
        , humanReadableName(humanReadableName)
        , frameId(frameId)
    {
    }

    v8::Local<v8::Context> context;
    bool isDefault;
    const String16 origin;
    const String16 humanReadableName;
    const String16 frameId;
};

using V8ContextInfoVector = protocol::Vector<V8ContextInfo>;

} // namespace blink

#endif // V8ContextInfo_h
