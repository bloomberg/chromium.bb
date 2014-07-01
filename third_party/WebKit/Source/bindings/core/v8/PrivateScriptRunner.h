// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrivateScriptRunner_h
#define PrivateScriptRunner_h

#include "wtf/text/WTFString.h"
#include <v8.h>

namespace WebCore {

class ScriptState;

class PrivateScriptRunner {
public:
    static v8::Handle<v8::Value> run(ScriptState*, String className, String functionName, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> argv[]);
};

} // namespace WebCore

#endif // V8PrivateScriptRunner_h
