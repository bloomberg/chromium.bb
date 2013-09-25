/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExceptionState_h
#define ExceptionState_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/V8ThrowException.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace WebCore {

typedef int ExceptionCode;

class ExceptionState {
    WTF_MAKE_NONCOPYABLE(ExceptionState);
public:
    explicit ExceptionState(v8::Isolate* isolate)
        : m_code(0)
        , m_isolate(isolate) { }

    virtual void throwDOMException(const ExceptionCode&,  const String& message);
    virtual void throwTypeError(const String& message = String());
    virtual void throwSecurityError(const String& sanitizedMessage, const String& unsanitizedMessage = String());

    // Please don't use this method. Use ::throwDOMException, and pass in a useful exception message.
    virtual void throwUninformativeAndGenericDOMException(const ExceptionCode& ec) { throwDOMException(ec, String()); };

    bool hadException() const { return !m_exception.isEmpty() || m_code; }
    void clearException();

    ExceptionCode code() { return m_code; }

    bool throwIfNeeded()
    {
        if (m_exception.isEmpty()) {
            if (!m_code)
                return false;
            throwUninformativeAndGenericDOMException(m_code);
        }

        V8ThrowException::throwError(m_exception.newLocal(m_isolate), m_isolate);
        return true;
    }

protected:
    ExceptionCode m_code;

private:
    void setException(v8::Handle<v8::Value>);

    ScopedPersistent<v8::Value> m_exception;
    v8::Isolate* m_isolate;
};

class TrackExceptionState : public ExceptionState {
public:
    TrackExceptionState(): ExceptionState(0) { }
    virtual void throwDOMException(const ExceptionCode&, const String& message) OVERRIDE FINAL;
    virtual void throwTypeError(const String& message = String()) OVERRIDE FINAL;
    virtual void throwSecurityError(const String& sanitizedMessage, const String& unsanitizedMessage = String()) OVERRIDE FINAL;
};

} // namespace WebCore

#endif // ExceptionState_h
