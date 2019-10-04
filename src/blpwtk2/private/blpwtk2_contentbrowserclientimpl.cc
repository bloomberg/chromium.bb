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

#include <base/json/json_reader.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/utf_string_conversions.h>
#include <base/threading/thread.h>
#include <base/threading/platform_thread.h>
#include <chrome/services/printing/public/mojom/constants.mojom.h>
#include <content/public/browser/browser_main_parts.h>
#include <content/public/browser/render_view_host.h>
#include <content/public/browser/render_process_host.h>
#include <content/public/browser/resource_dispatcher_host.h>
#include <content/public/browser/resource_dispatcher_host_delegate.h>
#include <content/public/browser/resource_request_info.h>
#include <content/public/browser/web_contents.h>
#include "content/public/common/service_manager_connection.h"
#include <content/public/common/service_names.mojom.h>
#include <content/public/common/url_constants.h>
#include <content/public/common/user_agent.h>
#include "chrome/app/chrome_content_browser_overlay_manifest.h"
#include "chrome/app/chrome_content_gpu_overlay_manifest.h"
#include "chrome/app/chrome_content_renderer_overlay_manifest.h"
#include "chrome/app/chrome_content_utility_overlay_manifest.h"
#include "chrome/app/chrome_renderer_manifest.h"
#include <chrome/browser/chrome_service.h>
#include <chrome/common/constants.mojom.h>
#include <chrome/grit/browser_resources.h>
#include "mojo/public/cpp/bindings/remote.h"
#include <services/service_manager/public/cpp/connector.h>
#include <ui/base/resource/resource_bundle.h>

#include "chrome/common/constants.mojom.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#include <chrome/browser/printing/printing_message_filter.h>

namespace blpwtk2 {
namespace {

                            // ====================================
                            // class ResourceDispatcherHostDelegate
                            // ====================================

class ResourceDispatcherHostDelegate : public content::ResourceDispatcherHostDelegate
{
    ResourceDispatcherHostDelegate() = default;
    DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHostDelegate);

  public:
    static ResourceDispatcherHostDelegate& Get();
};

                            // ------------------------------------
                            // class ResourceDispatcherHostDelegate
                            // ------------------------------------

ResourceDispatcherHostDelegate& ResourceDispatcherHostDelegate::Get()
{
    static ResourceDispatcherHostDelegate instance;
    return instance;
}

}  // close unnamed namespace


                        // ------------------------------
                        // class ContentBrowserClientImpl
                        // ------------------------------

ContentBrowserClientImpl::ContentBrowserClientImpl()
{
}

ContentBrowserClientImpl::~ContentBrowserClientImpl()
{
}

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters)
{
    auto main_parts = std::make_unique<BrowserMainParts>();
    main_parts->AddParts(ChromeService::GetInstance()->CreateExtraParts());
    return main_parts;
}

bool ContentBrowserClientImpl::ShouldEnableStrictSiteIsolation()
{
    return false;
}

void ContentBrowserClientImpl::RenderProcessWillLaunch(
    content::RenderProcessHost *host,
    service_manager::mojom::ServiceRequest* service_request)
{
    DCHECK(Statics::isInBrowserMainThread());

    int id = host->GetID();
    host->AddFilter(new printing::PrintingMessageFilter(id, nullptr));

    // Start a new instance of chrome_renderer service for the "to be"
    // launched renderer process.  This is a requirement for chrome services
    mojo::PendingRemote<service_manager::mojom::Service> service;
    *service_request = service.InitWithNewPipeAndPassReceiver();
    service_manager::Identity renderer_identity = host->GetChildIdentity();
    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    ChromeService::GetInstance()->connector()->RegisterServiceInstance(
        service_manager::Identity(chrome::mojom::kRendererServiceName,
                                  renderer_identity.instance_group(),
                                  renderer_identity.instance_id(),
                                  renderer_identity.globally_unique_id()),
        std::move(service), metadata.BindNewPipeAndPassReceiver());
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

void ContentBrowserClientImpl::ResourceDispatcherHostCreated()
{
    content::ResourceDispatcherHost::Get()->SetDelegate(
        &ResourceDispatcherHostDelegate::Get());
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
    mojo::OutgoingInvitation* broker_client_invitation,
    const std::string& service_token)
{
    d_broker_client_invitation = broker_client_invitation;
}

mojo::OutgoingInvitation* ContentBrowserClientImpl::GetClientInvitation() const
{
    return d_broker_client_invitation.load();
}

std::vector<service_manager::Manifest>
ContentBrowserClientImpl::GetExtraServiceManifests()
{
    return std::vector<service_manager::Manifest>{};
}

base::Optional<service_manager::Manifest> ContentBrowserClientImpl::GetServiceManifestOverlay(
        base::StringPiece name)
{
  if (name == content::mojom::kBrowserServiceName) {
    return GetChromeContentBrowserOverlayManifest();
  } else if (name == content::mojom::kGpuServiceName) {
    return GetChromeContentGpuOverlayManifest();
  } else if (name == content::mojom::kRendererServiceName) {
    return GetChromeContentRendererOverlayManifest();
  } else if (name == content::mojom::kUtilityServiceName) {
    return GetChromeContentUtilityOverlayManifest();
  }

  return base::nullopt;
}

void ContentBrowserClientImpl::RunServiceInstanceOnIOThread(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service>* receiver) {
  // needed for chrome services
  if (identity.name() == chrome::mojom::kServiceName) {
    ChromeService::GetInstance()->CreateChromeServiceRequestHandler().Run(
        std::move(*receiver));
    return;
  }
}

std::string ContentBrowserClientImpl::GetUserAgent() const
{
    // include Chrome in our user-agent because some sites actually look for
    // this.  For example, google's "Search as you type" feature.
    return content::BuildUserAgentFromProduct("BlpWtk/" BB_PATCH_VERSION " Chrome/" CHROMIUM_VERSION);
}

}  // close namespace blpwtk2

// vim: ts=4 et

