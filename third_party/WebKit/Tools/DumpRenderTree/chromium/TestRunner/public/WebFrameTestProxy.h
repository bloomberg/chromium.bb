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

namespace WebTestRunner {

// Templetized wrapper around RenderFrameImpl objects, which implement
// the WebFrameClient interface.
template<class Base, typename P, typename R>
class WebFrameTestProxy : public Base, public WebTestProxyBase {
public:
    WebFrameTestProxy(P p, R r)
        : Base(p, r)
    {
    }
    virtual ~WebFrameTestProxy() { }

    void setBaseProxy(WebTestProxyBase* proxy)
    {
        m_baseProxy = proxy;
    }

    // WebFrameClient implementation.
    virtual void willPerformClientRedirect(WebKit::WebFrame* frame, const WebKit::WebURL& from, const WebKit::WebURL& to, double interval, double fireTime)
    {
        m_baseProxy->willPerformClientRedirect(frame, from, to, interval, fireTime);
        Base::willPerformClientRedirect(frame, from, to, interval, fireTime);
    }
    virtual void didCancelClientRedirect(WebKit::WebFrame* frame)
    {
        m_baseProxy->didCancelClientRedirect(frame);
        Base::didCancelClientRedirect(frame);
    }
    virtual void didStartProvisionalLoad(WebKit::WebFrame* frame)
    {
        m_baseProxy->didStartProvisionalLoad(frame);
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame* frame)
    {
        m_baseProxy->didReceiveServerRedirectForProvisionalLoad(frame);
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        // If the test finished, don't notify the embedder of the failed load,
        // as we already destroyed the document loader.
        if (m_baseProxy->didFailProvisionalLoad(frame, error))
            return;
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame, bool isNewNavigation)
    {
        m_baseProxy->didCommitProvisionalLoad(frame, isNewNavigation);
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(WebKit::WebFrame* frame, const WebKit::WebString& title, WebKit::WebTextDirection direction)
    {
        m_baseProxy->didReceiveTitle(frame, title, direction);
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didChangeIcon(WebKit::WebFrame* frame, WebKit::WebIconURL::Type iconType)
    {
        m_baseProxy->didChangeIcon(frame, iconType);
        Base::didChangeIcon(frame, iconType);
    }
    virtual void didFinishDocumentLoad(WebKit::WebFrame* frame)
    {
        m_baseProxy->didFinishDocumentLoad(frame);
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(WebKit::WebFrame* frame)
    {
        m_baseProxy->didHandleOnloadEvents(frame);
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        m_baseProxy->didFailLoad(frame, error);
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(WebKit::WebFrame* frame)
    {
        m_baseProxy->didFinishLoad(frame);
        Base::didFinishLoad(frame);
    }
    virtual void didChangeLocationWithinPage(WebKit::WebFrame* frame)
    {
        m_baseProxy->didChangeLocationWithinPage(frame);
        Base::didChangeLocationWithinPage(frame);
    }
    virtual void didDisplayInsecureContent(WebKit::WebFrame* frame)
    {
        m_baseProxy->didDisplayInsecureContent(frame);
        Base::didDisplayInsecureContent(frame);
    }
    virtual void didRunInsecureContent(WebKit::WebFrame* frame, const WebKit::WebSecurityOrigin& origin, const WebKit::WebURL& insecureURL)
    {
        m_baseProxy->didRunInsecureContent(frame, origin, insecureURL);
        Base::didRunInsecureContent(frame, origin, insecureURL);
    }
    virtual void didDetectXSS(WebKit::WebFrame* frame, const WebKit::WebURL& insecureURL, bool didBlockEntirePage)
    {
        m_baseProxy->didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
    virtual void willRequestResource(WebKit::WebFrame* frame, const WebKit::WebCachedURLRequest& request)
    {
        m_baseProxy->willRequestResource(frame, request);
        Base::willRequestResource(frame, request);
    }
    virtual WebKit::WebURLError cannotHandleRequestError(WebKit::WebFrame* frame, const WebKit::WebURLRequest& request)
    {
        return m_baseProxy->cannotHandleRequestError(frame, request);
    }
    virtual void didCreateDataSource(WebKit::WebFrame* frame, WebKit::WebDataSource* ds)
    {
        m_baseProxy->didCreateDataSource(frame, ds);
        Base::didCreateDataSource(frame, ds);
    }
    virtual void willSendRequest(WebKit::WebFrame* frame, unsigned identifier, WebKit::WebURLRequest& request, const WebKit::WebURLResponse& redirectResponse)
    {
        m_baseProxy->willSendRequest(frame, identifier, request, redirectResponse);
        Base::willSendRequest(frame, identifier, request, redirectResponse);
    }
    virtual void didReceiveResponse(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLResponse& response)
    {
        m_baseProxy->didReceiveResponse(frame, identifier, response);
        Base::didReceiveResponse(frame, identifier, response);
    }
    virtual void didChangeResourcePriority(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLRequest::Priority& priority)
    {
        m_baseProxy->didChangeResourcePriority(frame, identifier, priority);
        Base::didChangeResourcePriority(frame, identifier, priority);
    }
    virtual void didFinishResourceLoad(WebKit::WebFrame* frame, unsigned identifier)
    {
        m_baseProxy->didFinishResourceLoad(frame, identifier);
        Base::didFinishResourceLoad(frame, identifier);
    }
    virtual void didFailResourceLoad(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLError& error)
    {
        m_baseProxy->didFailResourceLoad(frame, identifier, error);
        Base::didFailResourceLoad(frame, identifier, error);
    }
    virtual void unableToImplementPolicyWithError(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        m_baseProxy->unableToImplementPolicyWithError(frame, error);
        Base::unableToImplementPolicyWithError(frame, error);
    }
    virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(WebKit::WebFrame* frame, const WebKit::WebURLRequest& request, WebKit::WebNavigationType type, WebKit::WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        WebKit::WebNavigationPolicy policy = m_baseProxy->decidePolicyForNavigation(frame, request, type, defaultPolicy, isRedirect);
        if (policy == WebKit::WebNavigationPolicyIgnore)
            return policy;
        return Base::decidePolicyForNavigation(frame, request, type, defaultPolicy, isRedirect);
    }
    virtual bool willCheckAndDispatchMessageEvent(WebKit::WebFrame* sourceFrame, WebKit::WebFrame* targetFrame, WebKit::WebSecurityOrigin target, WebKit::WebDOMMessageEvent event)
    {
        if (m_baseProxy->willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event))
            return true;
        return Base::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event);
    }
    virtual WebKit::WebColorChooser* createColorChooser(WebKit::WebColorChooserClient* client, const WebKit::WebColor& color)
    {
        return m_baseProxy->createColorChooser(client, color);
    }


private:
    WebTestProxyBase* m_baseProxy;
};

}

#endif // WebTestProxy_h
