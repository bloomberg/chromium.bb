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

#ifndef INCLUDED_BLPWTK2_CONTENTBROWSERCLIENTIMPL_H
#define INCLUDED_BLPWTK2_CONTENTBROWSERCLIENTIMPL_H

#include <blpwtk2_config.h>

#include <content/public/browser/content_browser_client.h>
#include <content/public/browser/browser_context.h>
#include <content/public/browser/render_process_host.h>
#include <net/url_request/url_request_job_factory.h>
#include <services/service_manager/public/cpp/binder_registry.h>

#include <atomic>

namespace content {
    class AssociatedInterfaceRegistry;
    class ServiceManagerConnection;
}

namespace blpwtk2 {

                        // ==============================
                        // class ContentBrowserClientImpl
                        // ==============================

// This is our implementation of the content::ContentBrowserClient interface.
// This interface allows us to add hooks to the "browser" portion of the
// content module.  This is created as part of the startup process of
// BrowserMainRunner.
class ContentBrowserClientImpl : public content::ContentBrowserClient {
    DISALLOW_COPY_AND_ASSIGN(ContentBrowserClientImpl);
    
    // The following are the in-process render thread's info
    std::atomic<mojo::OutgoingInvitation*> d_broker_client_invitation{nullptr};

  public:
    // CREATORS
    explicit ContentBrowserClientImpl();
    ~ContentBrowserClientImpl() final;

    void RenderProcessWillLaunch(content::RenderProcessHost* host,
        service_manager::mojom::ServiceRequest* service_request) override;

    // Notifies that a render process will be created. This is called
    // before the content layer adds its own BrowserMessageFilters, so
    // that the embedder's IPC filters have priority.
    std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
        const content::MainFunctionParams& parameters) override;
        // A non-nullptr return value is needed because
        // BrowserMainLoop::PreShutdown() assumes a non-nullptr

    void OverrideWebkitPrefs(content::RenderViewHost *render_view_host,
                             content::WebPreferences *prefs) override;
        // Called by WebContents to override the WebKit preferences that are
        // used by the renderer. The content layer will add its own settings,
        // and then it's up to the embedder to update it if it wants.
    
    bool ShouldEnableStrictSiteIsolation() override;
        // Allows the embedder to programmatically control whether the
        // --site-per-process mode of Site Isolation should be used.
        //
        // Note that for correctness, the same value should be consistently returned.
        // See also https://crbug.com/825369

    bool SupportsInProcessRenderer() override;
        // Returns true whether the embedder supports in-process renderers or
        // not.  When running "in process", the browser maintains a
        // RenderProcessHost which communicates to a RenderProcess which is
        // instantiated in the same process with the Browser. All IPC between
        // the Browser and the Renderer is the same, it's just not crossing a
        // process boundary. This returns false by default. If implementations
        // return true, they must also implement StartInProcessRendererThread
        // and StopInProcessRendererThread.

    void ResourceDispatcherHostCreated() override;

    content::WebContentsViewDelegate *GetWebContentsViewDelegate(
        content::WebContents *webContents) override;
        // If content creates the WebContentsView implementation, it will ask
        // the embedder to return an (optional) delegate to customize it. The
        // view will own the delegate.

    bool IsHandledURL(const GURL& url) override;
        // Returns whether a specified URL is handled by the embedder's
        // internal protocol handlers.

    content::DevToolsManagerDelegate *GetDevToolsManagerDelegate() override;
        // Creates a new DevToolsManagerDelegate. The caller owns the returned
        // value.  It's valid to return NULL.

    void ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        blink::AssociatedInterfaceRegistry* associated_registry,
        content::RenderProcessHost* render_process_host) override;

    // Start the in-process renderer thread.  This will only ever be called if
    // SupportsInProcessRenderer() returns true.
    void StartInProcessRendererThread(
        mojo::OutgoingInvitation* broker_client_invitation,
        const std::string& service_token) override;

    // Use build-time C++ manifests instead of runtime JSON manifests
    // https://chromium-review.googlesource.com/c/chromium/src/+/1423354
    std::vector<service_manager::Manifest> GetExtraServiceManifests() override;

    mojo::OutgoingInvitation* GetClientInvitation() const;

    base::Optional<service_manager::Manifest> GetServiceManifestOverlay(
            base::StringPiece name) override;

    void RunServiceInstanceOnIOThread(
        const service_manager::Identity& identity,
        mojo::PendingReceiver<service_manager::mojom::Service>* receiver)
        override;

    // Returns the user agent.  Content may cache this value.
    std::string GetUserAgent() const override;

};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONTENTBROWSERCLIENTIMPL_H

// vim: ts=4 et

