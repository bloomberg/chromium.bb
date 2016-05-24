// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceLocation_h
#define SourceLocation_h

#include "core/CoreExport.h"
#include "platform/v8_inspector/public/V8StackTrace.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class TracedValue;
class V8StackTrace;

class CORE_EXPORT SourceLocation {
public:
    // Zero lineNumber and columnNumber mean unknown. Captures current stack trace.
    static PassOwnPtr<SourceLocation> capture(const String& url, unsigned lineNumber, unsigned columnNumber);

    // Shortcut when location is unknown. Tries to capture call stack or parsing location if available.
    static PassOwnPtr<SourceLocation> capture(ExecutionContext* = nullptr);

    static PassOwnPtr<SourceLocation> create(const String& url, unsigned lineNumber, unsigned columnNumber, PassOwnPtr<V8StackTrace>, int scriptId = 0);
    ~SourceLocation();

    String url() const { return m_url; }
    unsigned lineNumber() const { return m_lineNumber; }
    unsigned columnNumber() const { return m_columnNumber; }
    PassOwnPtr<V8StackTrace> takeStackTrace() { return std::move(m_stackTrace); }
    int scriptId() const { return m_scriptId; }

private:
    SourceLocation(const String& url, unsigned lineNumber, unsigned columnNumber, PassOwnPtr<V8StackTrace>, int scriptId);

    String m_url;
    unsigned m_lineNumber;
    unsigned m_columnNumber;
    OwnPtr<V8StackTrace> m_stackTrace;
    int m_scriptId;
};

} // namespace blink

#endif // SourceLocation_h
