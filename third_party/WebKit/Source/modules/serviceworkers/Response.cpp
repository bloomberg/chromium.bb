// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Response.h"

#include "bindings/v8/Dictionary.h"
#include "modules/serviceworkers/ResponseInit.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace WebCore {

PassRefPtr<Response> Response::create()
{
    return create(Dictionary());
}

PassRefPtr<Response> Response::create(const Dictionary& responseInit)
{
    return adoptRef(new Response(ResponseInit(responseInit)));
}

PassRefPtr<HeaderMap> Response::headers() const
{
    // FIXME: Implement. Spec will eventually whitelist allowable headers.
    return m_headers;
}

void Response::populateWebServiceWorkerResponse(blink::WebServiceWorkerResponse& response)
{
    response.setStatus(status());
    response.setStatusText(statusText());
    response.setHeaders(m_headers->headerMap());
}

Response::Response(const ResponseInit& responseInit)
    : m_status(responseInit.status)
    , m_statusText(responseInit.statusText)
    , m_headers(responseInit.headers)
{
    ScriptWrappable::init(this);

    if (!m_headers)
        m_headers = HeaderMap::create();
}

} // namespace WebCore
