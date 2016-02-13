// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IgnoreExceptionsScope_h
#define IgnoreExceptionsScope_h

#include "wtf/OwnPtr.h"

namespace blink {

class V8Debugger;
class IgnoreExceptionsScopeImpl;

class IgnoreExceptionsScope {
public:
    explicit IgnoreExceptionsScope(V8Debugger*);
    ~IgnoreExceptionsScope();

private:
    OwnPtr<IgnoreExceptionsScopeImpl> m_impl;
};

} // namespace blink

#endif // IgnoreExceptionsScope_h
