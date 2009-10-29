/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef EmptyWebFrameClientImpl_h
#define EmptyWebFrameClientImpl_h

#include "WebFrameClient.h"
#include "WebURLError.h"

namespace WebKit {

// Extend from this if you only need to override a few WebFrameClient methods.
class EmptyWebFrameClient : public WebFrameClient {
public:
    virtual WebPlugin* createPlugin(WebFrame*, const WebPluginParams&) { return 0; }
    virtual WebWorker* createWorker(WebFrame*, WebWorkerClient*) { return 0; }
    virtual WebMediaPlayer* createMediaPlayer(WebFrame*, WebMediaPlayerClient*) { return 0; }
    virtual void willClose(WebFrame*) {}
    virtual void loadURLExternally(WebFrame*, const WebURLRequest&, WebNavigationPolicy) {}
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebFrame*, const WebURLRequest&, WebNavigationType, const WebNode&,
        WebNavigationPolicy defaultPolicy, bool) { return defaultPolicy; }
    virtual bool canHandleRequest(WebFrame*, const WebURLRequest&) { return true; }
    virtual WebURLError cannotHandleRequestError(WebFrame*, const WebURLRequest&) { return WebURLError(); }
    virtual WebURLError cancelledError(WebFrame*, const WebURLRequest&) { return WebURLError(); }
    virtual void unableToImplementPolicyWithError(WebFrame*, const WebURLError&) {}
    virtual void willSubmitForm(WebFrame*, const WebForm&) {}
    virtual void willPerformClientRedirect(WebFrame*, const WebURL&, const WebURL&, double, double) {}
    virtual void didCancelClientRedirect(WebFrame*) {}
    virtual void didCompleteClientRedirect(WebFrame*, const WebURL& from) {}
    virtual void didCreateDataSource(WebFrame*, WebDataSource*) {}
    virtual void didStartProvisionalLoad(WebFrame*) {}
    virtual void didReceiveServerRedirectForProvisionalLoad(WebFrame*) {}
    virtual void didFailProvisionalLoad(WebFrame*, const WebURLError&) {}
    virtual void didReceiveDocumentData(WebFrame*, const char*, size_t, bool&) {}
    virtual void didCommitProvisionalLoad(WebFrame*, bool) {}
    virtual void didClearWindowObject(WebFrame*) {}
    virtual void didCreateDocumentElement(WebFrame*) {}
    virtual void didReceiveTitle(WebFrame*, const WebString&) {}
    virtual void didFinishDocumentLoad(WebFrame*) {}
    virtual void didHandleOnloadEvents(WebFrame*) {}
    virtual void didFailLoad(WebFrame*, const WebURLError&) {}
    virtual void didFinishLoad(WebFrame*) {}
    virtual void didChangeLocationWithinPage(WebFrame*, bool) {}
    virtual void didUpdateCurrentHistoryItem(WebFrame*) {}
    virtual void assignIdentifierToRequest(WebFrame*, unsigned, const WebURLRequest&) {}
    virtual void willSendRequest(WebFrame*, unsigned, WebURLRequest&, const WebURLResponse&) {}
    virtual void didReceiveResponse(WebFrame*, unsigned, const WebURLResponse&) {}
    virtual void didFinishResourceLoad(WebFrame*, unsigned) {}
    virtual void didFailResourceLoad(WebFrame*, unsigned, const WebURLError&) {}
    virtual void didLoadResourceFromMemoryCache(WebFrame*, const WebURLRequest&, const WebURLResponse&) {}
    virtual void didDisplayInsecureContent(WebFrame*) {}
    virtual void didRunInsecureContent(WebFrame*, const WebSecurityOrigin&) {}
    virtual void didExhaustMemoryAvailableForScript(WebFrame*) {}
    virtual void didCreateScriptContext(WebFrame*) {}
    virtual void didDestroyScriptContext(WebFrame*) {}
    virtual void didCreateIsolatedScriptContext(WebFrame*) {}
    virtual void didChangeContentsSize(WebFrame*, const WebSize&) {}
    virtual void reportFindInPageMatchCount(int, int, bool) {}
    virtual void reportFindInPageSelection(int, int, const WebRect&) {}
};

} // namespace WebKit

#endif
