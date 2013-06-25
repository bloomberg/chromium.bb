/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBDatabaseError_h
#define IDBDatabaseError_h

#include "core/dom/DOMCoreException.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class IDBDatabaseError : public RefCounted<IDBDatabaseError> {
public:
    static PassRefPtr<IDBDatabaseError> create(ExceptionCode ec)
    {
        return adoptRef(new IDBDatabaseError(ec));
    }

    static PassRefPtr<IDBDatabaseError> create(ExceptionCode ec, const String& message)
    {
        return adoptRef(new IDBDatabaseError(ec, message));
    }

    ~IDBDatabaseError() { }

    unsigned short code() const { return DOMCoreException::getLegacyErrorCode(m_exceptionCode); }
    ExceptionCode exceptionCode() const { return m_exceptionCode; }
    const String& message() const { return m_message; }
    const String name() const { return DOMCoreException::getErrorName(m_exceptionCode); };

private:
    IDBDatabaseError(unsigned short code)
        : m_exceptionCode(code), m_message(DOMCoreException::getErrorDescription(code)) { }
    IDBDatabaseError(unsigned short code, const String& message)
        : m_exceptionCode(code), m_message(message) { }

    const ExceptionCode m_exceptionCode;
    const String m_message;
};

} // namespace WebCore

#endif // IDBDatabaseError_h
