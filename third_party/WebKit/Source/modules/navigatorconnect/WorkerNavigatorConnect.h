// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerNavigatorConnect_h
#define WorkerNavigatorConnect_h

#include "modules/navigatorconnect/NavigatorConnect.h"

namespace blink {

class WorkerNavigator;

class WorkerNavigatorConnect {
public:
    static ScriptPromise connect(ScriptState* scriptState, WorkerNavigator&, const String& url)
    {
        return NavigatorConnect::connect(scriptState, url);
    }
};

} // namespace blink

#endif // WorkerNavigatorConnect_h
