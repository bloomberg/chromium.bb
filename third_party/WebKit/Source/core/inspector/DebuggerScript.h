// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DebuggerScript_h
#define DebuggerScript_h

#include <v8.h>

namespace blink {

v8::Local<v8::Object> compileDebuggerScript(v8::Isolate*);

} // namespace blink


#endif // !defined(DebuggerScript_h)
