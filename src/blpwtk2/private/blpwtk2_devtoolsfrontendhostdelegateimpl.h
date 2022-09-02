/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_DEVTOOLSFRONTENDHOSTDELEGATE_H
#define INCLUDED_BLPWTK2_DEVTOOLSFRONTENDHOSTDELEGATE_H

#include <blpwtk2_config.h>

#include <base/memory/weak_ptr.h>
#include <base/values.h>
#include <content/public/browser/web_contents_observer.h>
#include <content/public/browser/devtools_agent_host_client.h>
#include <content/public/browser/devtools_frontend_host.h>
#include <net/url_request/url_fetcher_delegate.h>

#include "base/containers/unique_ptr_adapters.h"

namespace content {
    class DevToolsAgentHost;
}  // close namespace content

namespace blpwtk2 {

class URLRequestContextGetterImpl;

// TODO: document this
class DevToolsFrontendHostDelegateImpl final
    : public content::WebContentsObserver,
      public content::DevToolsAgentHostClient {
  public:
    DevToolsFrontendHostDelegateImpl(content::WebContents* inspectorContents,
                                     content::WebContents* inspectedContents);
    ~DevToolsFrontendHostDelegateImpl() override;

    void inspectElementAt(const POINT& point);

    void CallClientFunction(
        const std::string& object_name,
        const std::string& method_name,
        const base::Value arg1 = {},
        const base::Value arg2 = {},
        const base::Value arg3 = {},
        base::OnceCallback<void(base::Value)> cb = base::NullCallback());

    void SendMessageAck(int request_id, base::Value arg);

    // ======== WebContentsObserver overrides ============

    void RenderFrameCreated(content::RenderFrameHost* renderFrameHost) override;
    void PrimaryMainDocumentElementAvailable() override;
    void WebContentsDestroyed() override;


    // ======== DevToolsFrontendHost message callback ===========

    void HandleMessageFromDevToolsFrontend(base::Value message);

    // ========= DevToolsAgentHostClient overrides =========
    void DispatchProtocolMessage(content::DevToolsAgentHost* agentHost,
                                 base::span<const uint8_t> message) override;
    void AgentHostClosed(content::DevToolsAgentHost* agentHost) override;

  private:
    class NetworkResourceLoader;

    std::set<std::unique_ptr<NetworkResourceLoader>, base::UniquePtrComparator>
      loaders_;

    base::DictionaryValue preferences_;

    using ExtensionsAPIs = std::map<std::string, std::string>;
    ExtensionsAPIs extensions_api_;

    content::WebContents* d_inspectedContents;
    scoped_refptr<content::DevToolsAgentHost> agent_host_;
    scoped_refptr<URLRequestContextGetterImpl> d_requestContextGetter;
    std::unique_ptr<content::DevToolsFrontendHost> d_frontendHost;
    using PendingRequestsMap = std::map<const net::URLFetcher*, int>;
    PendingRequestsMap d_pendingRequests;
    base::DictionaryValue d_preferences;

    POINT d_inspectElementPoint;
    bool d_inspectElementPointPending;
    base::WeakPtrFactory<DevToolsFrontendHostDelegateImpl> d_weakFactory;
};


}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_DEVTOOLSFRONTENDHOSTDELEGATE_H

// vim: ts=4 et

