// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorConnect_h
#define NavigatorConnect_h

#include "bindings/core/v8/ScriptPromise.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Navigator;
class ScriptState;

class NavigatorConnect {
public:
    static ScriptPromise connect(ScriptState* scriptState, Navigator&, const String& url)
    {
        return connect(scriptState, url);
    }

    static ScriptPromise connect(ScriptState*, const String& url);
};

} // namespace blink

#endif // NavigatorConnect_h
