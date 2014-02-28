// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Response_h
#define Response_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

struct ResponseInit;

class Response FINAL : public ScriptWrappable, public RefCounted<Response> {
public:
    static PassRefPtr<Response> create();
    static PassRefPtr<Response> create(const Dictionary& responseInit);
    ~Response() { };

    unsigned short statusCode() { return m_statusCode; }
    void setStatusCode(unsigned short statusCode) { m_statusCode = statusCode; }

    String statusText() { return m_statusText; }
    void setStatusText(const String& statusText) { m_statusText = statusText; }

    String method() { return m_method; }
    void setMethod(const String& method) { m_method = method; }

    Dictionary* headers();
    void headers(const Dictionary&);

private:
    explicit Response(const ResponseInit&);
    unsigned short m_statusCode;
    String m_statusText;
    String m_method;
    Dictionary m_headers;
};

} // namespace WebCore

#endif // Response_h
