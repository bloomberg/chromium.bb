// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace blink {

void WebServiceWorkerResponse::setStatusCode(unsigned short statusCode)
{
    m_statusCode = statusCode;
}

unsigned short WebServiceWorkerResponse::statusCode() const
{
    return m_statusCode;
}

void WebServiceWorkerResponse::setStatusText(const WebString& statusText)
{
    m_statusText = statusText;
}

WebString WebServiceWorkerResponse::statusText() const
{
    return m_statusText;
}

void WebServiceWorkerResponse::setMethod(const WebString& method)
{
    m_method = method;
}

WebString WebServiceWorkerResponse::method() const
{
    return m_method;
}

} // namespace blink
