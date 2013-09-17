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

#ifndef WebFrameTestProxy_h
#define WebFrameTestProxy_h

#include "WebTestProxy.h"
#include "public/platform/WebNonCopyable.h"

namespace WebTestRunner {

// Templetized wrapper around RenderFrameImpl objects, which implement
// the WebFrameClient interface.
template<class Base, typename P, typename R>
class WebFrameTestProxy : public Base, public WebKit::WebNonCopyable {
public:
    WebFrameTestProxy(P p, R r)
        : Base(p, r)
        , m_baseProxy(0)
        , m_version(0) { }

    virtual ~WebFrameTestProxy() { }

    void setBaseProxy(WebTestProxyBase* proxy)
    {
        m_baseProxy = proxy;
    }

    void setVersion(int version)
    {
        m_version = version;
    }

    WebKit::WebPlugin* createPlugin(WebKit::WebFrame* frame, const WebKit::WebPluginParams& params)
    {
        WebKit::WebPlugin* plugin = m_baseProxy->createPlugin(frame, params);
        if (plugin)
            return plugin;
        return Base::createPlugin(frame, params);
    }

    // WebFrameClient implementation.
    virtual void didStartProvisionalLoad(WebKit::WebFrame* frame)
    {
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame* frame)
    {
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame, bool isNewNavigation)
    {
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(WebKit::WebFrame* frame, const WebKit::WebString& title, WebKit::WebTextDirection direction)
    {
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didChangeIcon(WebKit::WebFrame* frame, WebKit::WebIconURL::Type iconType)
    {
        Base::didChangeIcon(frame, iconType);
    }
    virtual void didFinishDocumentLoad(WebKit::WebFrame* frame)
    {
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(WebKit::WebFrame* frame)
    {
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(WebKit::WebFrame* frame)
    {
        Base::didFinishLoad(frame);
    }
    virtual void didDetectXSS(WebKit::WebFrame* frame, const WebKit::WebURL& insecureURL, bool didBlockEntirePage)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
    virtual void didDispatchPingLoader(WebKit::WebFrame* frame, const WebKit::WebURL& url)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didDispatchPingLoader(frame, url);
        Base::didDispatchPingLoader(frame, url);
    }
    virtual void willRequestResource(WebKit::WebFrame* frame, const WebKit::WebCachedURLRequest& request)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->willRequestResource(frame, request);
        Base::willRequestResource(frame, request);
    }
    virtual void didCreateDataSource(WebKit::WebFrame* frame, WebKit::WebDataSource* ds)
    {
        Base::didCreateDataSource(frame, ds);
    }
    virtual void willSendRequest(WebKit::WebFrame* frame, unsigned identifier, WebKit::WebURLRequest& request, const WebKit::WebURLResponse& redirectResponse)
    {
        if (m_version > 1)
            m_baseProxy->willSendRequest(frame, identifier, request, redirectResponse);
        Base::willSendRequest(frame, identifier, request, redirectResponse);
    }
    virtual void didReceiveResponse(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLResponse& response)
    {
        if (m_version > 1)
            m_baseProxy->didReceiveResponse(frame, identifier, response);
        Base::didReceiveResponse(frame, identifier, response);
    }
    virtual void didChangeResourcePriority(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLRequest::Priority& priority)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didChangeResourcePriority(frame, identifier, priority);
        Base::didChangeResourcePriority(frame, identifier, priority);
    }
    virtual void didFinishResourceLoad(WebKit::WebFrame* frame, unsigned identifier)
    {
        Base::didFinishResourceLoad(frame, identifier);
    }
    virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(WebKit::WebFrame* frame, WebKit::WebDataSource::ExtraData* extraData, const WebKit::WebURLRequest& request, WebKit::WebNavigationType type, WebKit::WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        return Base::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
    }
    virtual bool willCheckAndDispatchMessageEvent(WebKit::WebFrame* sourceFrame, WebKit::WebFrame* targetFrame, WebKit::WebSecurityOrigin target, WebKit::WebDOMMessageEvent event)
    {
        if (m_baseProxy->willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event))
            return true;
        return Base::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event);
    }

private:
    WebTestProxyBase* m_baseProxy;

    // This is used to incrementally change code between Blink and Chromium.
    // It is used instead of a #define and is set by layouttest_support when
    // creating this object.
    int m_version;
};

}

#endif // WebTestProxy_h
