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

#include "wtf/Noncopyable.h"
#include <v8.h>

namespace WebCore {

typedef int ExceptionCode;

class ExceptionState {
    WTF_MAKE_NONCOPYABLE(ExceptionState);
public:
    explicit ExceptionState(v8::Isolate* isolate)
        : m_code(0)
        , m_exceptionThrown(false)
        , m_isolate(isolate) { }

    virtual void throwDOMException(const ExceptionCode&,  const char* message = 0);
    virtual void throwTypeError(const char* message = 0);

    bool hadException() const { return m_exceptionThrown || m_code; }

    bool throwIfNeeded()
    {
        if (m_code) {
            throwDOMException(m_code);
            return true;
        }
        return m_exceptionThrown;
    }

    // FIXME: Remove the rest of the public methods/operators once the transition is done.
    typedef void* ExceptionState::*UnspecifiedBoolType;
    operator UnspecifiedBoolType*() const
    {
        return m_code ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0;
    }

    operator ExceptionCode& () { return m_code; }

    ExceptionState& operator=(const ExceptionCode& ec)
    {
        throwDOMException(ec);
        return *this;
    }

protected:
    ExceptionCode m_code;

private:
    bool m_exceptionThrown;
    v8::Isolate* m_isolate;
};

class NonThrowExceptionState : public ExceptionState {
public:
    NonThrowExceptionState();
    virtual void throwDOMException(const ExceptionCode&, const char* = 0) OVERRIDE FINAL;
    virtual void throwTypeError(const char* = 0) OVERRIDE FINAL;
};

} // namespace WebCore

#endif // ExceptionState_h
