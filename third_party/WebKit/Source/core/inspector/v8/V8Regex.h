// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Regex_h
#define V8Regex_h

#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class V8DebuggerImpl;

enum MultilineMode {
    MultilineDisabled,
    MultilineEnabled
};

class V8Regex {
    USING_FAST_MALLOC(V8Regex);
    WTF_MAKE_NONCOPYABLE(V8Regex);
public:
    V8Regex(V8DebuggerImpl*, const String&, TextCaseSensitivity, MultilineMode = MultilineDisabled);
    int match(const String&, int startFrom = 0, int* matchLength = 0) const;
    bool isValid() const { return !m_regex.IsEmpty(); }

private:
    V8DebuggerImpl* m_debugger;
    v8::Global<v8::RegExp> m_regex;
};

} // namespace blink

#endif // V8Regex_h
