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

#include <blpwtk2_profileimpl.h>

#include <blpwtk2_browsercontextimpl.h>
#include <blpwtk2_webviewproxy.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webviewclientimpl.h>
#include <blpwtk2_mainmessagepump.h>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <ipc/ipc_sender.h>
#include <ipc/ipc_sync_channel.h>
#include <content/public/renderer/render_thread.h>
#include <content/common/service_manager/child_connection.h>
#include <mojo/public/cpp/bindings/strong_binding.h>
#include <services/service_manager/public/cpp/connector.h>
#include <third_party/blink/renderer/core/page/bb_window_hooks.h>

namespace blpwtk2 {

                        // -----------------
                        // class ProfileImpl
                        // -----------------

static std::set<Profile*> g_instances;

// static
Profile *ProfileImpl::anyInstance()
{
    return !g_instances.empty()? *(g_instances.begin()) : nullptr;
}

ProfileImpl::ProfileImpl(MainMessagePump *pump,
                         unsigned int     pid,
                         bool             launchDevToolsServer)
    : d_numWebViews(0)
    , d_processId(pid)
    , d_pump(pump)
{
    static const std::string SERVICE_NAME("content_browser");
    g_instances.insert(this);

    content::RenderThread* renderThread = content::RenderThread::Get();
	renderThread->GetConnector()->BindInterface(SERVICE_NAME, &d_hostPtr);
    DCHECK(0 != pid);
    d_hostPtr->bindProcess(pid, launchDevToolsServer);

    blink::BBWindowHooks::ProfileHooks hooks;
    hooks.getGpuInfo = base::BindRepeating(&ProfileImpl::getGpuInfo, base::Unretained(this));

    blink::BBWindowHooks::InstallProfileHooks(hooks);
}

ProfileImpl::~ProfileImpl()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!g_instances.empty());

    // Releasing the Mojo interface pointer will incur an IPC message to
    // the host.  If this process had some open webviews, the host will
    // likely send a ViewMsg_Close message to delete the RenderWidget
    // instances in this process.  The RenderWidget may then destroy the
    // compositor if it's no longer needed.  The compositor may hold
    // references to shared memory that it needs to release before it's
    // destroyed.
    //
    // We take the following approach to ensure a clean shutdown:
    //  - flush the event loop. this will make sure the IPC message is
    //    sent to the host
    //  - sleep for a short period. this will give the host some time
    //    to send a response
    //  - flush the event loop again. this will make sure the IPC
    //    response from the host is processed. the "clean up" task
    //    will eventually be sent to the compositor, at which point
    //    it will start cleaning up its resources

    d_hostPtr.reset();
    d_pump->flush();
    Sleep(50);
    d_pump->flush();

    g_instances.erase(this);
}

void ProfileImpl::incrementWebViewCount()
{
    DCHECK(Statics::isInApplicationMainThread());
    ++d_numWebViews;
}

void ProfileImpl::decrementWebViewCount()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(0 < d_numWebViews);
    --d_numWebViews;
}

unsigned int ProfileImpl::getProcessId() const
{
    return d_processId;
}

// blpwtk2::Profile overrides
void ProfileImpl::destroy()
{
    delete this;
}

String ProfileImpl::createHostChannel(unsigned int     pid,
                                      bool             isolated,
                                      const StringRef& profileDir)
{
    std::string hostChannel;

    if (d_hostPtr->createHostChannel(
                pid,
                isolated,
                std::string(profileDir.data(), profileDir.size()),
                &hostChannel)) {
        return String(hostChannel);
    }
    else {
        return String();
    }
}

String ProfileImpl::registerNativeViewForStreaming(NativeView view)
{
    std::string result;

    if (d_hostPtr->registerNativeViewForStreaming(
                reinterpret_cast<unsigned int>(view), &result)) {
        return String(result);
    }
    else {
        return String();
    }
}

String ProfileImpl::registerScreenForStreaming(NativeScreen screen)
{
    std::string result;

    if (d_hostPtr->registerScreenForStreaming(
                reinterpret_cast<unsigned int>(screen), &result)) {

        return String(result);
    }
    else {
        return String();
    }
}

// static
static void onWebViewCreated(
        WebViewProxy                *proxy,
        WebViewDelegate             *delegate,
        mojom::WebViewHostPtr       *webViewHostPtr,
        mojom::WebViewClientRequest  webViewClientRequest,
        int                          status)
{
    DCHECK(0 == status);
    if (status) {
        static_cast<WebView*>(proxy)->destroy();
        proxy = nullptr;
    }
    else {
        // Create a webview client and webview proxy.  They both have a
        // reference to one another so that when one goes away, it can tell
        // the other to dispose dangling references.
        std::unique_ptr<WebViewClientImpl> webViewClientImpl =
            std::make_unique<WebViewClientImpl>(std::move(*webViewHostPtr),
                                                proxy);

        proxy->setClient(webViewClientImpl.get());

        // Bind the webview client to the request from process host.  This
        // will make its lifetime managed by Mojo.
        mojo::MakeStrongBinding(std::move(webViewClientImpl),
                                std::move(webViewClientRequest));
    }

    delegate->created(proxy);
    delete webViewHostPtr;
}

void ProfileImpl::createWebView(WebViewDelegate            *delegate,
                                const WebViewCreateParams&  params)
{
    DCHECK(Statics::isInApplicationMainThread());

    const mojom::WebViewCreateParams *createParams =
        getWebViewCreateParamsImpl(params);

    // Create a new instance of WebViewProxy.
    WebViewProxy *proxy = new WebViewProxy(delegate, this);

    // Ask the process host to create a webview host. 
    mojom::WebViewHostPtr *webViewHostPtr =
        new mojom::WebViewHostPtr;

    auto taskRunner = base::MessageLoopCurrent::Get()->task_runner();

    d_hostPtr->createWebView(
        mojo::MakeRequest(webViewHostPtr, taskRunner),
        createParams->Clone(),
        base::Bind(
            &onWebViewCreated,
            proxy,
            delegate,
            webViewHostPtr));
}

void ProfileImpl::addHttpProxy(ProxyType        type,
                               const StringRef& host,
                               int              port)
{
    d_hostPtr->addHttpProxy(static_cast<mojom::ProxyConfigType>(type),
                            std::string(host.data(), host.size()),
                            port);
}

void ProfileImpl::addHttpsProxy(ProxyType        type,
                                const StringRef& host,
                                int              port)
{
    d_hostPtr->addHttpsProxy(static_cast<mojom::ProxyConfigType>(type),
                             std::string(host.data(), host.size()),
                             port);
}

void ProfileImpl::addFtpProxy(ProxyType        type,
                              const StringRef& host,
                              int              port)
{
    d_hostPtr->addFtpProxy(static_cast<mojom::ProxyConfigType>(type),
                           std::string(host.data(), host.size()),
                           port);
}

void ProfileImpl::addFallbackProxy(ProxyType        type,
                                   const StringRef& host,
                                   int              port)
{
    d_hostPtr->addFallbackProxy(static_cast<mojom::ProxyConfigType>(type),
                                std::string(host.data(), host.size()),
                                port);
}

void ProfileImpl::clearHttpProxies()
{
    d_hostPtr->clearHttpProxies();
}

void ProfileImpl::clearHttpsProxies()
{
    d_hostPtr->clearHttpsProxies();
}

void ProfileImpl::clearFtpProxies()
{
    d_hostPtr->clearFtpProxies();
}

void ProfileImpl::clearFallbackProxies()
{
    d_hostPtr->clearFallbackProxies();
}

void ProfileImpl::addBypassRule(const StringRef& rule)
{
    d_hostPtr->addBypassRule(std::string(rule.data(), rule.size()));
}

void ProfileImpl::clearBypassRules()
{
    d_hostPtr->clearBypassRules();
}

void ProfileImpl::setPacUrl(const StringRef& url)
{
    d_hostPtr->setPacUrl(std::string(url.data(), url.size()));
}



// patch section: spellcheck


// patch section: printing


// patch section: diagnostics
void ProfileImpl::dumpDiagnostics(DiagnosticInfoType type,
                                  const StringRef&   path)
{
    d_hostPtr->dumpDiagnostics(static_cast<int>(type),
                               std::string(path.data(), path.size()));
}

std::string ProfileImpl::getGpuInfo()
{
    std::string diagnostics;

    if (d_hostPtr->getGpuInfo(&diagnostics)) {
        return diagnostics;
    }
    else {
        return "";
    }
}

// patch section: embedder ipc


// patch section: web cache



}  // close namespace blpwtk2

// vim: ts=4 et

