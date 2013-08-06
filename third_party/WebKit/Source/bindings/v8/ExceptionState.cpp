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

#include "config.h"
#include "bindings/v8/ExceptionState.h"

#include "bindings/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"

namespace WebCore {

void ExceptionState::clearException()
{
    m_code = 0;
    m_exception.clear();
}

void ExceptionState::throwDOMException(const ExceptionCode& ec, const String& message)
{
    ASSERT(ec);
    ASSERT(m_isolate);
    m_code = ec;
    setException(V8ThrowException::createDOMException(ec, message, m_isolate));
}

void ExceptionState::setException(v8::Handle<v8::Value> exception)
{
    // FIXME: Assert that exception is not empty?
    if (exception.IsEmpty()) {
        clearException();
        return;
    }

    m_exception.set(m_isolate, exception);
}

void ExceptionState::throwTypeError(const String& message)
{
    ASSERT(m_isolate);
    m_code = TypeError;
    setException(V8ThrowException::createTypeError(message, m_isolate));
}

void TrackExceptionState::throwDOMException(const ExceptionCode& ec, const String& message)
{
    m_code = ec;
}

void TrackExceptionState::throwTypeError(const String&)
{
    m_code = TypeError;
}

} // namespace WebCore
