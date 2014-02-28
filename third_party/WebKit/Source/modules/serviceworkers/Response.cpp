// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Response.h"

#include "bindings/v8/Dictionary.h"
#include "modules/serviceworkers/ResponseInit.h"
#include "platform/NotImplemented.h"

namespace WebCore {

PassRefPtr<Response> Response::create()
{
    return create(Dictionary());
}

PassRefPtr<Response> Response::create(const Dictionary& responseInit)
{
    return adoptRef(new Response(ResponseInit(responseInit)));
}

void Response::headers(const Dictionary& headers)
{
    notImplemented();
}

Dictionary* Response::headers()
{
    // FIXME: Implement. Spec will eventually whitelist allowable headers.
    return &m_headers;
}

Response::Response(const ResponseInit& responseInit)
    : m_statusCode(responseInit.statusCode)
    , m_statusText(responseInit.statusText)
    , m_method(responseInit.method)
    , m_headers(responseInit.headers)
{
    ScriptWrappable::init(this);
}

} // namespace WebCore
