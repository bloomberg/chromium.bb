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

#include <blpwtk2_contentbrowserclientimpl.h>

#include <blpwtk2_browsercontextimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_urlrequestcontextgetterimpl.h>
#include <blpwtk2_webcontentsviewdelegateimpl.h>
#include <blpwtk2_devtoolsmanagerdelegateimpl.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_processhostimpl.h>
#include <blpwtk2_browsermainparts.h>
#include <blpwtk2_requestinterceptorimpl.h>

#include <base/json/json_reader.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/utf_string_conversions.h>
#include <base/threading/thread.h>
#include <base/threading/platform_thread.h>
#include <content/public/browser/browser_main_parts.h>
#include <content/public/browser/render_view_host.h>
#include <content/public/browser/render_process_host.h>
#include <content/public/browser/web_contents.h>
#include "content/public/common/service_manager_connection.h"
#include <content/public/common/service_names.mojom.h>
#include <content/public/common/url_constants.h>
#include <content/public/common/user_agent.h>
#include "content/browser/builtin_service_manifests.h"
#include "chrome/app/chrome_content_browser_overlay_manifest.h"
#include <chrome/grit/browser_resources.h>
#include "mojo/public/cpp/bindings/remote.h"
#include <net/url_request/url_request_job_factory_impl.h>
#include <services/service_manager/public/cpp/connector.h>
#include <ui/base/resource/resource_bundle.h>

#include "components/spellcheck/common/spellcheck.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#include <chrome/browser/printing/printing_message_filter.h>

namespace blpwtk2 {

                        // ------------------------------
                        // class ContentBrowserClientImpl
                        // ------------------------------

ContentBrowserClientImpl::ContentBrowserClientImpl() :
    d_interceptor(std::make_unique<RequestInterceptorImpl>())
{
    net::URLRequestJobFactoryImpl::SetInterceptorForTesting(d_interceptor.get());
}

ContentBrowserClientImpl::~ContentBrowserClientImpl()
{
    net::URLRequestJobFactoryImpl::SetInterceptorForTesting(nullptr);
}

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters)
{
    auto main_parts = std::make_unique<BrowserMainParts>();
    return main_parts;
}

bool ContentBrowserClientImpl::ShouldEnableStrictSiteIsolation()
{
    return false;
}

void ContentBrowserClientImpl::RenderProcessWillLaunch(
    content::RenderProcessHost *host)
{
    int id = host->GetID();
    host->AddFilter(new printing::PrintingMessageFilter(id, nullptr));
}

void ContentBrowserClientImpl::OverrideWebkitPrefs(
    content::RenderViewHost *render_view_host,
    content::WebPreferences *prefs)
{
    content::WebContents* webContents =
        content::WebContents::FromRenderViewHost(render_view_host);
    DCHECK(webContents);

    WebViewImpl* webViewImpl =
        static_cast<WebViewImpl*>(webContents->GetDelegate());
    DCHECK(webViewImpl);

    webViewImpl->overrideWebkitPrefs(prefs);
}

bool ContentBrowserClientImpl::SupportsInProcessRenderer()
{
    return Statics::isInProcessRendererEnabled;
}

content::WebContentsViewDelegate*
ContentBrowserClientImpl::GetWebContentsViewDelegate(content::WebContents* webContents)
{
    return new WebContentsViewDelegateImpl(webContents);
}

bool ContentBrowserClientImpl::IsHandledURL(const GURL& url)
{
    if (!url.is_valid())
        return false;
    DCHECK_EQ(url.scheme(), base::ToLowerASCII(url.scheme()));
    // Keep in sync with ProtocolHandlers added by
    // URLRequestContextGetterImpl::GetURLRequestContext().
    static const char* const kProtocolList[] = {
        url::kBlobScheme,
        url::kFileSystemScheme,
        content::kChromeUIScheme,
        content::kChromeDevToolsScheme,
        url::kDataScheme,
        url::kFileScheme,
    };
    for (size_t i = 0; i < base::size(kProtocolList); ++i) {
        if (url.scheme() == kProtocolList[i])
            return true;
    }
    return false;
}

content::DevToolsManagerDelegate *ContentBrowserClientImpl::GetDevToolsManagerDelegate()
{
    return new DevToolsManagerDelegateImpl();
}

void ContentBrowserClientImpl::ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        blink::AssociatedInterfaceRegistry* associated_registry,
        content::RenderProcessHost* render_process_host)
{
    ProcessHostImpl::registerMojoInterfaces(registry);
}

void ContentBrowserClientImpl::StartInProcessRendererThread(
    mojo::OutgoingInvitation* broker_client_invitation, int renderer_client_id)
{
    d_broker_client_invitation = broker_client_invitation;
    d_render_client_id = renderer_client_id;
}

mojo::OutgoingInvitation* ContentBrowserClientImpl::GetClientInvitation() const
{
    return d_broker_client_invitation.load();
}

std::vector<service_manager::Manifest>
ContentBrowserClientImpl::GetExtraServiceManifests()
{
    return content::GetBuiltinServiceManifests();
}

base::Optional<service_manager::Manifest> ContentBrowserClientImpl::GetServiceManifestOverlay(
        base::StringPiece name)
{
  if (name == content::mojom::kBrowserServiceName) {
    return GetChromeContentBrowserOverlayManifest();
  }
  return base::nullopt;
}

std::string ContentBrowserClientImpl::GetUserAgent()
{
    // include Chrome in our user-agent because some sites actually look for
    // this.  For example, google's "Search as you type" feature.
    return content::BuildUserAgentFromProduct("BlpWtk/" BB_PATCH_VERSION " Chrome/" CHROMIUM_VERSION);
}

void ContentBrowserClientImpl::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service)
{
    return content::ContentBrowserClient::OnNetworkServiceCreated(network_service);
}

void ContentBrowserClientImpl::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params)
{
    DCHECK(context);
    BrowserContextImpl* pContextImpl = static_cast<BrowserContextImpl*>(context);
    return pContextImpl->ConfigureNetworkContextParams(GetUserAgent(), network_context_params);
}

bool ContentBrowserClientImpl::ShouldLockToOrigin(content::BrowserContext* browser_context,
	                                              const GURL& effective_url)
{
    return false;
}

}  // close namespace blpwtk2

// vim: ts=4 et

