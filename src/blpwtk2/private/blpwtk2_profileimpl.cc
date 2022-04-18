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
#include <base/task/single_thread_task_executor.h>
#include <ipc/ipc_sender.h>
#include <ipc/ipc_sync_channel.h>
#include <content/public/renderer/render_thread.h>
#include <services/service_manager/public/cpp/connector.h>
#include <services/service_manager/public/cpp/service_filter.h>


// patch section: dump diagnostics



// patch section: web cache flush



#include <mojo/public/cpp/bindings/self_owned_receiver.h>

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
                         bool             launchDevToolsServer,
                         blink::ThreadSafeBrowserInterfaceBrokerProxy* broker)
    : d_numWebViews(0)
    , d_processId(pid)
    , d_pump(pump)
{
    static const std::string SERVICE_NAME("content_browser");
    g_instances.insert(this);

    broker->GetInterface(d_hostPtr.BindNewPipeAndPassReceiver());
    DCHECK(0 != pid);
    d_hostPtr->bindProcess(pid, launchDevToolsServer);


    // patch section: dump diagnostics



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
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(view)), &result)) {
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
                static_cast<unsigned int>(reinterpret_cast<uintptr_t>(screen)), &result)) {

        return String(result);
    }
    else {
        return String();
    }
}

// static
static void onWebViewCreated(
        std::unique_ptr<WebViewProxy>  proxy,
        WebViewDelegate               *delegate,
        mojom::WebViewHostPtr          webViewHostPtr,
        mojom::WebViewClientRequest    webViewClientRequest,
        int                            status)
{
    DCHECK(0 == status);
    if (status) {
        static_cast<WebView*>(proxy.get())->destroy();
        delegate->created(nullptr);
    }
    else {
        // Create a webview client and webview proxy.  They both have a
        // reference to one another so that when one goes away, it can tell
        // the other to dispose dangling references.
        std::unique_ptr<WebViewClientImpl> webViewClientImpl =
            std::make_unique<WebViewClientImpl>(std::move(webViewHostPtr),
                                                proxy.get());

        proxy->setClient(webViewClientImpl.get());

        mojo::PendingReceiver<mojom::WebViewClient> webViewClientReceiver =
            std::move(webViewClientRequest);

        // Bind the webview client to the request from process host.  This
        // will make its lifetime managed by Mojo.
        mojo::MakeSelfOwnedReceiver(std::move(webViewClientImpl),
                                    std::move(webViewClientReceiver));

        delegate->created(proxy.get());
    }

    proxy.release();
}

void ProfileImpl::createWebView(WebViewDelegate            *delegate,
                                const WebViewCreateParams&  params)
{
    DCHECK(Statics::isInApplicationMainThread());

    const mojom::WebViewCreateParams *createParams =
        getWebViewCreateParamsImpl(params);

    // Create a new instance of WebViewProxy.
    auto proxy = std::make_unique<WebViewProxy>(delegate, this);

    // Ask the process host to create a webview host. 
    mojom::WebViewHostPtr webViewHostPtr;
    auto request = mojo::MakeRequest(&webViewHostPtr,
                                     base::ThreadTaskRunnerHandle::Get());

    d_hostPtr->createWebView(
        std::move(request),
        createParams->Clone(),
        base::BindOnce(
            &onWebViewCreated,
            std::move(proxy),
            base::Unretained(delegate),
            std::move(webViewHostPtr)));
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


// patch section: embedder ipc


// patch section: web cache


// patch section: memory diagnostics
std::size_t ProfileImpl::getDiscardableSharedMemoryBytes()
{
    uint32_t bytes;
    return d_hostPtr->getDiscardableSharedMemoryBytes(&bytes) ? bytes: 0;
}


// patch section: gpu



}  // close namespace blpwtk2

// vim: ts=4 et

