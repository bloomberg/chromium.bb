// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebServiceWorkerRequest.h"

namespace blink {

class WebServiceWorkerRequestPrivate : public RefCounted<WebServiceWorkerRequestPrivate> {
public:
    WebServiceWorkerRequestPrivate()
        : m_isReload(false) { }
    WebURL m_url;
    WebString m_method;
    blink::HTTPHeaderMap m_headers;
    blink::Referrer m_referrer;
    bool m_isReload;
};

WebServiceWorkerRequest::WebServiceWorkerRequest()
    : m_private(adoptRef(new WebServiceWorkerRequestPrivate))
{
}

void WebServiceWorkerRequest::reset()
{
    m_private.reset();
}

void WebServiceWorkerRequest::assign(const WebServiceWorkerRequest& other)
{
    m_private = other.m_private;
}

void WebServiceWorkerRequest::setURL(const WebURL& url)
{
    m_private->m_url = url;
}

WebURL WebServiceWorkerRequest::url() const
{
    return m_private->m_url;
}

void WebServiceWorkerRequest::setMethod(const WebString& method)
{
    m_private->m_method = method;
}

WebString WebServiceWorkerRequest::method() const
{
    return m_private->m_method;
}

void WebServiceWorkerRequest::setHeader(const WebString& key, const WebString& value)
{
    if (equalIgnoringCase(key, "referer"))
        return;
    m_private->m_headers.add(key, value);
}

const blink::HTTPHeaderMap& WebServiceWorkerRequest::headers() const
{
    return m_private->m_headers;
}

void WebServiceWorkerRequest::setReferrer(const WebString& referrer, WebReferrerPolicy referrerPolicy)
{
    m_private->m_referrer = blink::Referrer(referrer, static_cast<blink::ReferrerPolicy>(referrerPolicy));
}

const blink::Referrer& WebServiceWorkerRequest::referrer() const
{
    return m_private->m_referrer;
}

void WebServiceWorkerRequest::setIsReload(bool isReload)
{
    m_private->m_isReload = isReload;
}

bool WebServiceWorkerRequest::isReload() const
{
    return m_private->m_isReload;
}

} // namespace blink
