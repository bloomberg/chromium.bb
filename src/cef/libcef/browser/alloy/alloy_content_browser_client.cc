// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_content_browser_client.h"

#include <algorithm>
#include <utility>

#include "include/cef_version.h"
#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/alloy/alloy_browser_main.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_message_filter.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/net/throttle_handler.h"
#include "libcef/browser/net_service/cookie_manager_impl.h"
#include "libcef/browser/net_service/login_delegate.h"
#include "libcef/browser/net_service/proxy_url_loader_factory.h"
#include "libcef/browser/net_service/resource_request_handler_wrapper.h"
#include "libcef/browser/plugins/plugin_service_filter.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/browser/speech_recognition_manager_delegate.h"
#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/x509_certificate_impl.h"
#include "libcef/common/alloy/alloy_content_client.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/request_impl.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "cef/grit/cef_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/printing_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/version_info/version_info.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/storage_quota_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "net/base/auth.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/host/ppapi_host.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

#if defined(OS_LINUX)
#include "libcef/common/widevine_loader.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MAC)
#include "base/debug/leak_annotations.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_MAC)
#include "net/ssl/client_cert_store_mac.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#endif

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#endif

#if defined(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#endif

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
#endif

namespace {

class CefQuotaCallbackImpl : public CefRequestCallback {
 public:
  using CallbackType = content::QuotaPermissionContext::PermissionCallback;

  explicit CefQuotaCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  ~CefQuotaCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        RunNow(std::move(callback_), false);
      } else {
        CEF_POST_TASK(CEF_IOT, base::BindOnce(&CefQuotaCallbackImpl::RunNow,
                                              std::move(callback_), false));
      }
    }
  }

  void Continue(bool allow) override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), allow);
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::BindOnce(&CefQuotaCallbackImpl::Continue,
                                            this, allow));
    }
  }

  void Cancel() override { Continue(false); }

  CallbackType Disconnect() WARN_UNUSED_RESULT { return std::move(callback_); }

 private:
  static void RunNow(CallbackType callback, bool allow) {
    CEF_REQUIRE_IOT();
    std::move(callback).Run(
        allow ? content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW
              : content::QuotaPermissionContext::
                    QUOTA_PERMISSION_RESPONSE_DISALLOW);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefQuotaCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefQuotaCallbackImpl);
};

class CefAllowCertificateErrorCallbackImpl : public CefRequestCallback {
 public:
  typedef base::OnceCallback<void(content::CertificateRequestResultType)>
      CallbackType;

  explicit CefAllowCertificateErrorCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  ~CefAllowCertificateErrorCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), false);
      } else {
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefAllowCertificateErrorCallbackImpl::RunNow,
                           std::move(callback_), false));
      }
    }
  }

  void Continue(bool allow) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), allow);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::Bind(&CefAllowCertificateErrorCallbackImpl::Continue,
                               this, allow));
    }
  }

  void Cancel() override { Continue(false); }

  CallbackType Disconnect() WARN_UNUSED_RESULT { return std::move(callback_); }

 private:
  static void RunNow(CallbackType callback, bool allow) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(
        allow ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
              : content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefAllowCertificateErrorCallbackImpl);
};

class CefSelectClientCertificateCallbackImpl
    : public CefSelectClientCertificateCallback {
 public:
  explicit CefSelectClientCertificateCallbackImpl(
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : delegate_(std::move(delegate)) {}

  ~CefSelectClientCertificateCallbackImpl() {
    // If Select has not been called, call it with NULL to continue without any
    // client certificate.
    if (delegate_)
      DoSelect(nullptr);
  }

  void Select(CefRefPtr<CefX509Certificate> cert) override {
    if (delegate_)
      DoSelect(cert);
  }

 private:
  void DoSelect(CefRefPtr<CefX509Certificate> cert) {
    if (CEF_CURRENTLY_ON_UIT()) {
      RunNow(std::move(delegate_), cert);
    } else {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefSelectClientCertificateCallbackImpl::RunNow,
                         std::move(delegate_), cert));
    }
  }

  static void RunNow(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert) {
    CEF_REQUIRE_UIT();

    if (cert) {
      CefX509CertificateImpl* certImpl =
          static_cast<CefX509CertificateImpl*>(cert.get());
      certImpl->AcquirePrivateKey(base::BindOnce(
          &CefSelectClientCertificateCallbackImpl::RunWithPrivateKey,
          std::move(delegate), cert));
      return;
    }

    delegate->ContinueWithCertificate(nullptr, nullptr);
  }

  static void RunWithPrivateKey(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> key) {
    CEF_REQUIRE_UIT();
    DCHECK(cert);

    if (key) {
      CefX509CertificateImpl* certImpl =
          static_cast<CefX509CertificateImpl*>(cert.get());
      delegate->ContinueWithCertificate(certImpl->GetInternalCertObject(), key);
    } else {
      delegate->ContinueWithCertificate(nullptr, nullptr);
    }
  }

  std::unique_ptr<content::ClientCertificateDelegate> delegate_;

  IMPLEMENT_REFCOUNTING(CefSelectClientCertificateCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSelectClientCertificateCallbackImpl);
};

class CefQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  CefQuotaPermissionContext() {}

  // The callback will be dispatched on the IO thread.
  void RequestQuotaPermission(const content::StorageQuotaParams& params,
                              int render_process_id,
                              PermissionCallback callback) override {
    if (params.storage_type != blink::mojom::StorageType::kPersistent) {
      // To match Chrome behavior we only support requesting quota with this
      // interface for Persistent storage type.
      std::move(callback).Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
      return;
    }

    bool handled = false;

    CefRefPtr<AlloyBrowserHostImpl> browser =
        AlloyBrowserHostImpl::GetBrowserForFrameRoute(render_process_id,
                                                      params.render_frame_id);
    if (browser.get()) {
      CefRefPtr<CefClient> client = browser->GetClient();
      if (client.get()) {
        CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
        if (handler.get()) {
          CefRefPtr<CefQuotaCallbackImpl> callbackImpl(
              new CefQuotaCallbackImpl(std::move(callback)));
          handled = handler->OnQuotaRequest(
              browser.get(), params.origin_url.spec(), params.requested_size,
              callbackImpl.get());
          if (!handled) {
            // May return nullptr if the client has already executed the
            // callback.
            callback = callbackImpl->Disconnect();
          }
        }
      }
    }

    if (!handled && !callback.is_null()) {
      // Disallow the request by default.
      std::move(callback).Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    }
  }

 private:
  ~CefQuotaPermissionContext() override {}

  DISALLOW_COPY_AND_ASSIGN(CefQuotaPermissionContext);
};

#if defined(OS_POSIX) && !defined(OS_MAC)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    // Uploads will only occur if a non-empty crash URL is specified in
    // AlloyMainDelegate::InitCrashReporter.
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path,
                                            true /* upload */);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (!breakpad::IsCrashReporterEnabled())
    return -1;

  // Extensions have the same process type as renderers.
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MAC)

// From chrome/browser/plugins/chrome_content_browser_client_plugins_part.cc.
void BindPluginInfoHost(
    int render_process_id,
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginInfoHost> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;

  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<PluginInfoHostImpl>(render_process_id, profile),
      std::move(receiver));
}

base::FilePath GetRootCachePath() {
  // The CefContext::ValidateCachePath method enforces the requirement that all
  // cache_path values be either equal to or a child of root_cache_path.
  return base::FilePath(
      CefString(&CefContext::Get()->settings().root_cache_path));
}

// Register BrowserInterfaceBroker's GetInterface() handler callbacks for
// chrome-specific document-scoped interfaces.
// Stub implementations to silence "Empty binder for interface
// blink.mojom.[Name] for the frame/document scope" errors.
// Based on chrome/browser/chrome_browser_interface_binders.cc.
void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<blink::mojom::InsecureInputService>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<blink::mojom::InsecureInputService> receiver) {
      }));

  map->Add<blink::mojom::PrerenderProcessor>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<blink::mojom::PrerenderProcessor> receiver) {}));
}

}  // namespace

AlloyContentBrowserClient::AlloyContentBrowserClient() {
  plugin_service_filter_.reset(new CefPluginServiceFilter);
  content::PluginServiceImpl::GetInstance()->SetFilter(
      plugin_service_filter_.get());
}

AlloyContentBrowserClient::~AlloyContentBrowserClient() {}

std::unique_ptr<content::BrowserMainParts>
AlloyContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new AlloyBrowserMainParts(parameters);
  return base::WrapUnique(browser_main_parts_);
}

void AlloyContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  const int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->AddFilter(new CefBrowserMessageFilter(id));

  if (extensions::ExtensionsEnabled()) {
    host->AddFilter(new extensions::ExtensionMessageFilter(id, profile));
    host->AddFilter(
        new extensions::ExtensionsGuestViewMessageFilter(id, profile));
  }

  // If the renderer process crashes then the host may already have
  // CefBrowserInfoManager as an observer. Try to remove it first before adding
  // to avoid DCHECKs.
  host->RemoveObserver(CefBrowserInfoManager::GetInstance());
  host->AddObserver(CefBrowserInfoManager::GetInstance());

  // Forwards dynamic parameters to CefRenderThreadObserver.
  Profile* original_profile = profile->GetOriginalProfile();
  RendererUpdaterFactory::GetForProfile(original_profile)
      ->InitializeRenderer(host);
}

bool AlloyContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& effective_url) {
  if (!extensions::ExtensionsEnabled())
    return false;

  if (!effective_url.SchemeIs(extensions::kExtensionScheme))
    return false;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  if (!registry)
    return false;

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(effective_url.host());
  if (!extension)
    return false;

  // TODO(extensions): Extra checks required if type is TYPE_HOSTED_APP.

  // Hosted apps that have script access to their background page must use
  // process per site, since all instances can make synchronous calls to the
  // background window.  Other extensions should use process per site as well.
  return true;
}

// Based on
// ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess.
bool AlloyContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (!extensions::ExtensionsEnabled())
    return false;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->enabled_extensions()
          .GetExtensionOrAppByURL(effective_site_url);
  // Isolate all extensions.
  return extension != nullptr;
}

void AlloyContentBrowserClient::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (extensions::ExtensionsEnabled()) {
    extensions::URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
        browser_context, origin, is_for_isolated_world, factory_params);
  }
}

void AlloyContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  // Any schemes listed here are treated as WebUI schemes but do not get WebUI
  // bindings. Also, view-source is allowed for these schemes. WebUI schemes
  // will not be passed to HandleExternalProtocol.
}

void AlloyContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);

  additional_schemes->push_back(extensions::kExtensionScheme);
}

void AlloyContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(content::kChromeDevToolsScheme);
  additional_allowed_schemes->push_back(content::kChromeUIScheme);
}

bool AlloyContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return scheme::IsWebUIAllowedToMakeNetworkRequests(origin);
}

bool AlloyContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string& scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

  if (scheme::IsInternalHandledScheme(scheme))
    return true;

  return CefAppManager::Get()->HasCustomScheme(scheme);
}

void AlloyContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
  if (!extensions::ExtensionsEnabled())
    return;

  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  auto browser_context =
      static_cast<AlloyBrowserContext*>(site_instance->GetBrowserContext());

  extensions::ProcessMap::Get(browser_context)
      ->Insert(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());
}

void AlloyContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
  if (!extensions::ExtensionsEnabled())
    return;

  // May be NULL during shutdown.
  if (!extensions::ExtensionsBrowserClient::Get())
    return;

  // May be NULL during shutdown.
  if (!site_instance->HasProcess())
    return;

  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  auto browser_context =
      static_cast<AlloyBrowserContext*>(site_instance->GetBrowserContext());

  extensions::ProcessMap::Get(browser_context)
      ->Remove(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());
}

void AlloyContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto host_receiver = receiver.As<spellcheck::mojom::SpellCheckHost>()) {
    SpellCheckHostChromeImpl::Create(render_process_host->GetID(),
                                     std::move(host_receiver));
    return;
  }

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
  if (auto panel_host_receiver =
          receiver.As<spellcheck::mojom::SpellCheckPanelHost>()) {
    SpellCheckPanelHostImpl::Create(render_process_host->GetID(),
                                    std::move(panel_host_receiver));
    return;
  }
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
}

void AlloyContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine* browser_cmd = base::CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kDisablePackLoading,
#if defined(OS_MAC)
      switches::kFrameworkDirPath,
      switches::kMainBundlePath,
#endif
      switches::kLocalesDirPath,
      switches::kLogFile,
      switches::kLogSeverity,
      switches::kResourcesDirPath,
      embedder_support::kUserAgent,
      switches::kUserAgentProductAndVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));
  }

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kDisableExtensions,
        switches::kDisablePdfExtension,
        switches::kDisablePlugins,
        switches::kDisablePrintPreview,
        switches::kDisableScrollBounce,
        switches::kDisableSpellChecking,
        switches::kEnableSpeechInput,
        switches::kUncaughtExceptionStackSize,
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));

    if (extensions::ExtensionsEnabled()) {
      content::RenderProcessHost* process =
          content::RenderProcessHost::FromID(child_process_id);
      auto browser_context = process->GetBrowserContext();
      CefBrowserContext* cef_browser_context =
          process ? CefBrowserContext::FromBrowserContext(browser_context)
                  : nullptr;
      if (cef_browser_context) {
        if (cef_browser_context->IsPrintPreviewSupported()) {
          command_line->AppendSwitch(switches::kEnablePrintPreview);
        }

        // Based on ChromeContentBrowserClientExtensionsPart::
        // AppendExtraRendererCommandLineSwitches
        if (extensions::ProcessMap::Get(browser_context)
                ->Contains(process->GetID())) {
          command_line->AppendSwitch(extensions::switches::kExtensionProcess);
        }
      }
    }
  } else {
    // Propagate the following switches to non-renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kLang,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));
  }

#if defined(OS_LINUX)
  if (process_type == switches::kZygoteProcess) {
#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)
    if (!browser_cmd->HasSwitch(sandbox::policy::switches::kNoSandbox)) {
      // Pass the Widevine CDM path to the Zygote process. See comments in
      // CefWidevineLoader::AddContentDecryptionModules.
      const base::FilePath& cdm_path = CefWidevineLoader::GetInstance()->path();
      if (!cdm_path.empty())
        command_line->AppendSwitchPath(switches::kWidevineCdmPath, cdm_path);
    }
#endif

    if (browser_cmd->HasSwitch(switches::kBrowserSubprocessPath)) {
      // Force use of the sub-process executable path for the zygote process.
      const base::FilePath& subprocess_path =
          browser_cmd->GetSwitchValuePath(switches::kBrowserSubprocessPath);
      if (!subprocess_path.empty())
        command_line->SetProgram(subprocess_path);
    }
  }
#endif  // defined(OS_LINUX)

  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app.get()) {
    CefRefPtr<CefBrowserProcessHandler> handler =
        app->GetBrowserProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefCommandLineImpl> commandLinePtr(
          new CefCommandLineImpl(command_line, false, false));
      handler->OnBeforeChildProcessLaunch(commandLinePtr.get());
      commandLinePtr->Detach(nullptr);
    }
  }
}

std::string AlloyContentBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

scoped_refptr<network::SharedURLLoaderFactory>
AlloyContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  if (!SystemNetworkContextManager::GetInstance())
    return nullptr;

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network::mojom::NetworkContext*
AlloyContentBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance()->GetContext();
}

scoped_refptr<content::QuotaPermissionContext>
AlloyContentBrowserClient::CreateQuotaPermissionContext() {
  return new CefQuotaPermissionContext();
}

content::MediaObserver* AlloyContentBrowserClient::GetMediaObserver() {
  return CefMediaCaptureDevicesDispatcher::GetInstance();
}

content::SpeechRecognitionManagerDelegate*
AlloyContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSpeechInput))
    return new CefSpeechRecognitionManagerDelegate();

  return nullptr;
}

content::GeneratedCodeCacheSettings
AlloyContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  const base::FilePath& cache_path = context->GetPath();
  return content::GeneratedCodeCacheSettings(!cache_path.empty() /* enabled */,
                                             0 /* size */, cache_path);
}

void AlloyContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  CEF_REQUIRE_UIT();

  if (!is_main_frame_request) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the request
    // hard.
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
    return;
  }

  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(web_contents);
  if (!browser.get())
    return;
  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get())
    return;
  CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefSSLInfo> cef_ssl_info = new CefSSLInfoImpl(ssl_info);

  CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl(
      new CefAllowCertificateErrorCallbackImpl(std::move(callback)));

  bool proceed = handler->OnCertificateError(
      browser.get(), static_cast<cef_errorcode_t>(cert_error),
      request_url.spec(), cef_ssl_info, callbackImpl.get());
  if (!proceed) {
    // |callback| may be null if the user executed it despite returning false.
    callback = callbackImpl->Disconnect();
    if (!callback.is_null()) {
      std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
    }
  }
}

base::OnceClosure AlloyContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefRequestHandler> handler;
  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(web_contents);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get())
      handler = client->GetRequestHandler();
  }

  if (!handler.get()) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  CefRequestHandler::X509CertificateList certs;
  for (net::ClientCertIdentityList::iterator iter = client_certs.begin();
       iter != client_certs.end(); iter++) {
    certs.push_back(new CefX509CertificateImpl(std::move(*iter)));
  }

  CefRefPtr<CefSelectClientCertificateCallbackImpl> callbackImpl(
      new CefSelectClientCertificateCallbackImpl(std::move(delegate)));

  bool proceed = handler->OnSelectClientCertificate(
      browser.get(), cert_request_info->is_proxy,
      cert_request_info->host_and_port.host(),
      cert_request_info->host_and_port.port(), certs, callbackImpl.get());

  if (!proceed && !certs.empty()) {
    callbackImpl->Select(certs[0]);
  }
  return base::OnceClosure();
}

bool AlloyContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  CEF_REQUIRE_UIT();
  *no_javascript_access = false;

  return CefBrowserInfoManager::GetInstance()->CanCreateWindow(
      opener, target_url, referrer, frame_name, disposition, features,
      user_gesture, opener_suppressed, no_javascript_access);
}

void AlloyContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  auto rvh = web_contents->GetRenderViewHost();

  // Using RVH instead of RFH here because rvh->GetMainFrame() may be nullptr
  // when this method is called.
  renderer_prefs::PopulateWebPreferences(rvh, *prefs);

  if (rvh->GetWidget()->GetView()) {
    rvh->GetWidget()->GetView()->SetBackgroundColor(
        prefs->base_background_color);
  }
}

bool AlloyContentBrowserClient::OverrideWebPreferencesAfterNavigation(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  return renderer_prefs::PopulateWebPreferencesAfterNavigation(web_contents,
                                                               *prefs);
}

void AlloyContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  scheme::BrowserURLHandlerCreated(handler);
}

std::string AlloyContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

void AlloyContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  browser_host->GetPpapiHost()->AddHostFactoryFilter(
      std::unique_ptr<ppapi::host::HostFactory>(
          new ChromeBrowserPepperHostFactory(browser_host)));
}

std::unique_ptr<content::DevToolsManagerDelegate>
AlloyContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<CefDevToolsManagerDelegate>();
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
AlloyContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  throttle::NavigationThrottleList throttles;
  throttle::CreateThrottlesForNavigation(navigation_handle, throttles);
  return throttles;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AlloyContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  // Used to substitute View ID for PDF contents when using the PDF plugin.
  result.push_back(std::make_unique<PluginResponseInterceptorURLLoaderThrottle>(
      request.destination, frame_tree_node_id));

  Profile* profile = Profile::FromBrowserContext(browser_context);

  chrome::mojom::DynamicParams dynamic_params = {
      profile->GetPrefs()->GetBoolean(prefs::kForceGoogleSafeSearch),
      profile->GetPrefs()->GetInteger(prefs::kForceYouTubeRestrict),
      profile->GetPrefs()->GetString(prefs::kAllowedDomainsForApps)};
  result.push_back(
      std::make_unique<GoogleURLLoaderThrottle>(std::move(dynamic_params)));

  return result;
}

#if defined(OS_LINUX)
void AlloyContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_LINUX)

void AlloyContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* host) {
  associated_registry->AddInterface(
      base::BindRepeating(&BindPluginInfoHost, host->GetID()));
}

std::unique_ptr<net::ClientCertStore>
AlloyContentBrowserClient::CreateClientCertStore(
    content::BrowserContext* browser_context) {
  // Match the logic in ProfileNetworkContextService::CreateClientCertStore.
#if defined(USE_NSS_CERTS)
  // TODO: Add support for client implementation of crypto password dialog.
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory()));
#elif defined(OS_WIN)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif defined(OS_MAC)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#else
#error Unknown platform.
#endif
}

std::unique_ptr<content::LoginDelegate>
AlloyContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<net_service::LoginDelegate>(
      auth_info, web_contents, request_id, url,
      std::move(auth_required_callback));
}

void AlloyContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  if (!extensions::ExtensionsEnabled())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(), ukm_source_id,
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
}

void AlloyContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  if (!extensions::ExtensionsEnabled())
    return;

  auto factory = extensions::CreateExtensionURLLoaderFactory(render_process_id,
                                                             render_frame_id);
  if (factory)
    factories->emplace(extensions::kExtensionScheme, std::move(factory));

  content::RenderFrameHost* frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  extensions::CefExtensionWebContentsObserver* web_observer =
      extensions::CefExtensionWebContentsObserver::FromWebContents(
          web_contents);

  // There is nothing to do if no CefExtensionWebContentsObserver is attached
  // to the |web_contents|.
  if (!web_observer)
    return;

  const extensions::Extension* extension =
      web_observer->GetExtensionFromFrame(frame_host, false);
  if (!extension)
    return;

  std::vector<std::string> allowed_webui_hosts;
  // Support for chrome:// scheme if appropriate.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      extensions::Manifest::IsComponentLocation(extension->location())) {
    // Components of chrome that are implemented as extensions or platform apps
    // are allowed to use chrome://resources/ and chrome://theme/ URLs.
    // See also HasCrossOriginWhitelistEntry.
    allowed_webui_hosts.emplace_back(content::kChromeUIResourcesHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIThemeHost);
  }
  if (!allowed_webui_hosts.empty()) {
    factories->emplace(content::kChromeUIScheme,
                       content::CreateWebUIURLLoaderFactory(
                           frame_host, content::kChromeUIScheme,
                           std::move(allowed_webui_hosts)));
  }
}

bool AlloyContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    base::Optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  auto request_handler = net_service::CreateInterceptedRequestHandler(
      browser_context, frame, render_process_id,
      type == URLLoaderFactoryType::kNavigation,
      type == URLLoaderFactoryType::kDownload, request_initiator);

  net_service::ProxyURLLoaderFactory::CreateProxy(
      browser_context, factory_receiver, header_client,
      std::move(request_handler));
  return true;
}

void AlloyContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // Need to set up global NetworkService state before anything else uses it.
  DCHECK(SystemNetworkContextManager::GetInstance());
  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
}

void AlloyContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  // This method may be called during shutdown when using multi-threaded
  // message loop mode. In that case exit early to avoid crashes.
  if (!SystemNetworkContextManager::GetInstance()) {
    // This must match the value expected in
    // StoragePartitionImpl::InitNetworkContext.
    network_context_params->context_name = "magic_shutting_down";
    return;
  }

  auto cef_context = CefBrowserContext::FromBrowserContext(context);

  Profile* profile = cef_context->AsProfile();
  ProfileNetworkContextService* service =
      ProfileNetworkContextServiceFactory::GetForContext(profile);
  if (service) {
    service->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                           network_context_params,
                                           cert_verifier_creation_params);
  } else {
    // Set default params.
    network_context_params->user_agent = GetUserAgent();
    network_context_params->accept_language = GetApplicationLocale();
  }

  network_context_params->cookieable_schemes =
      cef_context->GetCookieableSchemes();

  // TODO(cef): Remove this and add required NetworkIsolationKeys,
  // this is currently not the case and this was not required pre M84.
  network_context_params->require_network_isolation_key = false;
}

// The sandbox may block read/write access from the NetworkService to
// directories that are not returned by this method.
std::vector<base::FilePath>
AlloyContentBrowserClient::GetNetworkContextsParentDirectory() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  DCHECK(!user_data_path.empty());

  const auto& root_cache_path = GetRootCachePath();

  // root_cache_path may sometimes be empty or a child of user_data_path, so
  // only return the one path in that case.
  if (root_cache_path.empty() || user_data_path.IsParent(root_cache_path)) {
    return {user_data_path};
  }

  return {user_data_path, root_cache_path};
}

bool AlloyContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::OnceGetter web_contents_getter,
    int child_id,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const base::Optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // Call the other HandleExternalProtocol variant.
  return false;
}

bool AlloyContentBrowserClient::HandleExternalProtocol(
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();

  // CefBrowserPlatformDelegate::HandleExternalProtocol may be called if
  // nothing handles the request.
  auto request_handler = net_service::CreateInterceptedRequestHandler(
      web_contents_getter, frame_tree_node_id, resource_request,
      base::Bind(CefBrowserPlatformDelegate::HandleExternalProtocol,
                 resource_request.url));

  net_service::ProxyURLLoaderFactory::CreateProxy(
      web_contents_getter, std::move(receiver), std::move(request_handler));
  return true;
}

std::unique_ptr<content::OverlayWindow>
AlloyContentBrowserClient::CreateWindowForPictureInPicture(
    content::PictureInPictureWindowController* controller) {
  // Note: content::OverlayWindow::Create() is defined by platform-specific
  // implementation in chrome/browser/ui/views. This layering hack, which goes
  // through //content and ContentBrowserClient, allows us to work around the
  // dependency constraints that disallow directly calling
  // chrome/browser/ui/views code either from here or from other code in
  // chrome/browser.
  return content::OverlayWindow::Create(controller);
}

void AlloyContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateChromeFrameBinders(map);

  if (!extensions::ExtensionsEnabled())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(extensions::kExtensionScheme))
    return;

  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  auto* extension = extensions::ExtensionRegistry::Get(browser_context)
                        ->enabled_extensions()
                        .GetByID(site.host());
  if (!extension)
    return;
  extensions::ExtensionsBrowserClient::Get()
      ->RegisterBrowserInterfaceBindersForFrame(map, render_frame_host,
                                                extension);
}

base::FilePath
AlloyContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return GetRootCachePath();
}

std::string AlloyContentBrowserClient::GetProduct() {
  return embedder_support::GetProduct();
}

std::string AlloyContentBrowserClient::GetChromeProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string AlloyContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

blink::UserAgentMetadata AlloyContentBrowserClient::GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list = {blink::UserAgentBrandVersion{
      version_info::GetProductName(), version_info::GetMajorVersionNumber()}};
  metadata.full_version = version_info::GetVersionNumber();
  metadata.platform = version_info::GetOSType();

  // TODO(mkwst): Poke at BuildUserAgentFromProduct to split out these pieces.
  metadata.architecture = "";
  metadata.model = "";

  return metadata;
}

base::flat_set<std::string>
AlloyContentBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    content::BrowserContext* browser_context) {
  base::flat_set<std::string> mime_types;
  auto map = PluginUtils::GetMimeTypeToExtensionIdMap(browser_context);
  for (const auto& pair : map)
    mime_types.insert(pair.first);
  return mime_types;
}

bool AlloyContentBrowserClient::ArePersistentMediaDeviceIDsAllowed(
    content::BrowserContext* browser_context,
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  // Persistent MediaDevice IDs are allowed if cookies are allowed.
  return CookieSettingsFactory::GetForProfile(
             Profile::FromBrowserContext(browser_context))
      ->IsCookieAccessAllowed(url, site_for_cookies, top_frame_origin);
}

bool AlloyContentBrowserClient::ShouldAllowPluginCreation(
    const url::Origin& embedder_origin,
    const content::PepperPluginInfo& plugin_info) {
  if (plugin_info.name == ChromeContentClient::kPDFInternalPluginName) {
    // Allow embedding the internal PDF plugin in the built-in PDF extension.
    if (embedder_origin.scheme() == extensions::kExtensionScheme &&
        embedder_origin.host() == extension_misc::kPdfExtensionId) {
      return true;
    }

    // Allow embedding the internal PDF plugin in chrome://print.
    if (embedder_origin ==
        url::Origin::Create(GURL(chrome::kChromeUIPrintURL))) {
      return true;
    }

    // Only allow the PDF plugin in the known, trustworthy origins that are
    // allowlisted above.  See also https://crbug.com/520422 and
    // https://crbug.com/1027173.
    return false;
  }

  return true;
}

CefRefPtr<CefRequestContextImpl> AlloyContentBrowserClient::request_context()
    const {
  return browser_main_parts_->request_context();
}

CefDevToolsDelegate* AlloyContentBrowserClient::devtools_delegate() const {
  return browser_main_parts_->devtools_delegate();
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentBrowserClient::background_task_runner() const {
  return browser_main_parts_->background_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentBrowserClient::user_visible_task_runner() const {
  return browser_main_parts_->user_visible_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentBrowserClient::user_blocking_task_runner() const {
  return browser_main_parts_->user_blocking_task_runner();
}

const extensions::Extension* AlloyContentBrowserClient::GetExtension(
    content::SiteInstance* site_instance) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!registry)
    return nullptr;
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
}
