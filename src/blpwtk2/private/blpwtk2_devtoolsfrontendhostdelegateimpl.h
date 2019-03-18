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

namespace content {
    class DevToolsAgentHost;
}  // close namespace content

namespace blpwtk2 {


// TODO: document this
class DevToolsFrontendHostDelegateImpl
    : public content::WebContentsObserver,
      public content::DevToolsAgentHostClient,
      public net::URLFetcherDelegate {
  public:
    DevToolsFrontendHostDelegateImpl(content::WebContents* inspectorContents,
                                     content::WebContents* inspectedContents);
    ~DevToolsFrontendHostDelegateImpl() final;

    void inspectElementAt(const POINT& point);

    void CallClientFunction(const std::string& function_name,
                            const base::Value* arg1,
                            const base::Value* arg2,
                            const base::Value* arg3);
    void SendMessageAck(int request_id, const base::Value* arg);

    // ======== WebContentsObserver overrides ============

    void RenderViewCreated(content::RenderViewHost* renderViewHost) override;
    void DocumentAvailableInMainFrame() override;
    void WebContentsDestroyed() override;


    // ======== DevToolsFrontendHost message callback ===========

    void HandleMessageFromDevToolsFrontend(const std::string& message);


    // ========= DevToolsAgentHostClient overrides =========
    void DispatchProtocolMessage(content::DevToolsAgentHost* agentHost,
                                 const std::string& message) override;
    void AgentHostClosed(content::DevToolsAgentHost* agentHost) override;

    // ========= net::URLFetcherDelegate overrides =========
    void OnURLFetchComplete(const net::URLFetcher* source) override;

  private:
    content::WebContents* d_inspectedContents;
    scoped_refptr<content::DevToolsAgentHost> d_agentHost;
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

