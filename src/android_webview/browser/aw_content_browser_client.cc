// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_content_browser_overlay_manifest.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_devtools_manager_delegate.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/aw_http_auth_handler.h"
#include "android_webview/browser/aw_quota_permission_context.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/aw_speech_recognition_manager_delegate.h"
#include "android_webview/browser/aw_web_contents_view_delegate.h"
#include "android_webview/browser/cookie_manager.h"
#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"
#include "android_webview/browser/network_service/aw_proxying_restricted_cookie_manager.h"
#include "android_webview/browser/network_service/aw_proxying_url_loader_factory.h"
#include "android_webview/browser/network_service/aw_url_loader_throttle.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_navigation_throttle.h"
#include "android_webview/browser/safe_browsing/aw_url_checker_delegate_impl.h"
#include "android_webview/browser/tracing/aw_tracing_delegate.h"
#include "android_webview/common/aw_content_client.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/content_capture/browser/content_capture_receiver_manager.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/policy/content/policy_blacklist_navigation_throttle.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/core/features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/android/network_library.h"
#include "net/http/http_util.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/display/display.h"
#include "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/spell_check_host_impl.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace android_webview {
namespace {
static bool g_should_create_thread_pool = true;
#if DCHECK_IS_ON()
// A boolean value to determine if the NetworkContext has been created yet. This
// exists only to check correctness: g_check_cleartext_permitted may only be set
// before the NetworkContext has been created (otherwise,
// g_check_cleartext_permitted won't have any effect).
bool g_created_network_context_params = false;
#endif

// On apps targeting API level O or later, check cleartext is enforced.
bool g_check_cleartext_permitted = false;

// TODO(sgurun) move this to its own file.
// This class filters out incoming aw_contents related IPC messages for the
// renderer process on the IPC thread.
class AwContentsMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit AwContentsMessageFilter(int process_id);

  // BrowserMessageFilter methods.
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnShouldOverrideUrlLoading(int routing_id,
                                  const base::string16& url,
                                  bool has_user_gesture,
                                  bool is_redirect,
                                  bool is_main_frame,
                                  bool* ignore_navigation);
  void OnSubFrameCreated(int parent_render_frame_id, int child_render_frame_id);

 private:
  ~AwContentsMessageFilter() override;

  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(AwContentsMessageFilter);
};

AwContentsMessageFilter::AwContentsMessageFilter(int process_id)
    : BrowserMessageFilter(AndroidWebViewMsgStart), process_id_(process_id) {}

AwContentsMessageFilter::~AwContentsMessageFilter() = default;

void AwContentsMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == AwViewHostMsg_ShouldOverrideUrlLoading::ID) {
    *thread = BrowserThread::UI;
  }
}

bool AwContentsMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwContentsMessageFilter, message)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_ShouldOverrideUrlLoading,
                        OnShouldOverrideUrlLoading)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_SubFrameCreated, OnSubFrameCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwContentsMessageFilter::OnShouldOverrideUrlLoading(
    int render_frame_id,
    const base::string16& url,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    bool* ignore_navigation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  *ignore_navigation = false;
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromID(process_id_, render_frame_id);
  if (client) {
    if (!client->ShouldOverrideUrlLoading(url, has_user_gesture, is_redirect,
                                          is_main_frame, ignore_navigation)) {
      // If the shouldOverrideUrlLoading call caused a java exception we should
      // always return immediately here!
      return;
    }
  } else {
    LOG(WARNING) << "Failed to find the associated render view host for url: "
                 << url;
  }
}

void AwContentsMessageFilter::OnSubFrameCreated(int parent_render_frame_id,
                                                int child_render_frame_id) {
  AwContentsIoThreadClient::SubFrameCreated(process_id_, parent_render_frame_id,
                                            child_render_frame_id);
}

// Helper method that checks the RenderProcessHost is still alive before hopping
// over to the IO thread.
void MaybeCreateSafeBrowsing(
    int rph_id,
    content::ResourceContext* resource_context,
    base::RepeatingCallback<scoped_refptr<safe_browsing::UrlCheckerDelegate>()>
        get_checker_delegate,
    mojo::PendingReceiver<safe_browsing::mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(rph_id);
  if (!render_process_host)
    return;

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&safe_browsing::MojoSafeBrowsingImpl::MaybeCreate, rph_id,
                     resource_context, std::move(get_checker_delegate),
                     std::move(receiver)));
}

}  // anonymous namespace

std::string GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string GetUserAgent() {
  // "Version/4.0" had been hardcoded in the legacy WebView.
  std::string product = "Version/4.0 " + GetProduct();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent)) {
    product += " Mobile";
  }
  return content::BuildUserAgentFromProductAndExtraOSInfo(
      product, "; wv", content::IncludeAndroidBuildNumber::Include);
}

// TODO(yirui): can use similar logic as in PrependToAcceptLanguagesIfNecessary
// in chrome/browser/android/preferences/pref_service_bridge.cc
// static
std::string AwContentBrowserClient::GetAcceptLangsImpl() {
  // Start with the current locale(s) in BCP47 format.
  std::string locales_string = AwContents::GetLocaleList();

  // If accept languages do not contain en-US, add in en-US which will be
  // used with a lower q-value.
  if (locales_string.find("en-US") == std::string::npos)
    locales_string += ",en-US";
  return locales_string;
}

// static
void AwContentBrowserClient::set_check_cleartext_permitted(bool permitted) {
#if DCHECK_IS_ON()
  DCHECK(!g_created_network_context_params);
#endif
  g_check_cleartext_permitted = permitted;
}

// static
bool AwContentBrowserClient::get_check_cleartext_permitted() {
  return g_check_cleartext_permitted;
}

AwContentBrowserClient::AwContentBrowserClient(
    AwFeatureListCreator* aw_feature_list_creator)
    : sniff_file_urls_(AwSettings::GetAllowSniffingFileUrls()),
      aw_feature_list_creator_(aw_feature_list_creator) {
  // |aw_feature_list_creator| should not be null. The AwBrowserContext will
  // take the PrefService owned by the creator as the Local State instead
  // of loading the JSON file from disk.
  DCHECK(aw_feature_list_creator_);
}

AwContentBrowserClient::~AwContentBrowserClient() {}

void AwContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  network::mojom::HttpAuthStaticParamsPtr auth_static_params =
      network::mojom::HttpAuthStaticParams::New();
  auth_static_params->supported_schemes = AwBrowserContext::GetAuthSchemes();
  content::GetNetworkService()->SetUpHttpAuth(std::move(auth_static_params));
}

void AwContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  DCHECK(context);

  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      AwBrowserProcess::GetInstance()->CreateHttpAuthDynamicParams());

  AwBrowserContext* aw_context = static_cast<AwBrowserContext*>(context);
  aw_context->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                            network_context_params,
                                            cert_verifier_creation_params);

  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote;
  network_context_params->cookie_manager =
      cookie_manager_remote.InitWithNewPipeAndPassReceiver();

#if DCHECK_IS_ON()
  g_created_network_context_params = true;
#endif

  // Pass the mojo::PendingRemote<network::mojom::CookieManager> to
  // android_webview::CookieManager, so it can implement its APIs with this mojo
  // CookieManager.
  aw_context->GetCookieManager()->SetMojoCookieManager(
      std::move(cookie_manager_remote));
}

AwBrowserContext* AwContentBrowserClient::InitBrowserContext() {
  browser_context_ = std::make_unique<AwBrowserContext>();
  return browser_context_.get();
}

std::unique_ptr<content::BrowserMainParts>
AwContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return std::make_unique<AwBrowserMainParts>(this);
}

content::WebContentsViewDelegate*
AwContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return AwWebContentsViewDelegate::Create(web_contents);
}

void AwContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  // Grant content: scheme access to the whole renderer process, since we impose
  // per-view access checks, and access is granted by default (see
  // AwSettings.mAllowContentUrlAccess).
  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      host->GetID(), url::kContentScheme);

  host->AddFilter(new AwContentsMessageFilter(host->GetID()));
  // WebView always allows persisting data.
  host->AddFilter(new cdm::CdmMessageFilterAndroid(true, false));
}

bool AwContentBrowserClient::IsExplicitNavigation(
    ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED);
}

bool AwContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  const std::string scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kWsScheme,
    url::kWssScheme,
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kDataScheme,
    url::kBlobScheme,
    url::kFileSystemScheme,
    content::kChromeUIScheme,
    url::kContentScheme,
  };
  if (scheme == url::kFileScheme) {
    // Return false for the "special" file URLs, so they can be loaded
    // even if access to file: scheme is not granted to the child process.
    return !IsAndroidSpecialFileUrl(url);
  }
  for (const char* supported_protocol : kProtocolList) {
    if (scheme == supported_protocol)
      return true;
  }
  return false;
}

bool AwContentBrowserClient::ForceSniffingFileUrlsForHtml() {
  return sniff_file_urls_;
}

void AwContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (!command_line->HasSwitch(switches::kSingleProcess)) {
    // The only kind of a child process WebView can have is renderer or utility.
    std::string process_type =
        command_line->GetSwitchValueASCII(switches::kProcessType);
    DCHECK(process_type == switches::kRendererProcess ||
           process_type == switches::kUtilityProcess)
        << process_type;
    // Pass crash reporter enabled state to renderer processes.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kEnableCrashReporter)) {
      command_line->AppendSwitch(::switches::kEnableCrashReporter);
    }
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kEnableCrashReporterForTesting)) {
      command_line->AppendSwitch(::switches::kEnableCrashReporterForTesting);
    }
  }
}

std::string AwContentBrowserClient::GetApplicationLocale() {
  return base::android::GetDefaultLocaleString();
}

std::string AwContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return GetAcceptLangsImpl();
}

gfx::ImageSkia AwContentBrowserClient::GetDefaultFavicon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(boliu): Bundle our own default favicon?
  return rb.GetImageNamed(IDR_DEFAULT_FAVICON).AsImageSkia();
}

bool AwContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    content::BrowserContext* context) {
  // WebView doesn't have a per-site policy for locally stored data,
  // instead AppCache can be disabled for individual WebViews.
  return true;
}

scoped_refptr<content::QuotaPermissionContext>
AwContentBrowserClient::CreateQuotaPermissionContext() {
  return new AwQuotaPermissionContext;
}

content::GeneratedCodeCacheSettings
AwContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  AwBrowserContext* browser_context = static_cast<AwBrowserContext*>(context);
  return content::GeneratedCodeCacheSettings(true, 0,
                                             browser_context->GetCacheDir());
}

void AwContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);
  bool cancel_request = true;
  // We only call the callback once but we must pass ownership to a function
  // that conditionally calls it.
  // TODO(estaab): Change AwContentsClientBridge::AllowCertificateError to
  //               return the callback if it doesn't call it.
  base::RepeatingCallback<void(content::CertificateRequestResultType)>
      repeating_callback = base::AdaptCallbackForRepeating(std::move(callback));
  if (client) {
    client->AllowCertificateError(cert_error, ssl_info.cert.get(), request_url,
                                  base::BindOnce(repeating_callback),
                                  &cancel_request);
  }
  if (cancel_request) {
    repeating_callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
}

base::OnceClosure AwContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (client)
    client->SelectClientCertificate(cert_request_info, std::move(delegate));
  return base::OnceClosure();
}

bool AwContentBrowserClient::CanCreateWindow(
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
  // We unconditionally allow popup windows at this stage and will give
  // the embedder the opporunity to handle displaying of the popup in
  // WebContentsDelegate::AddContents (via the
  // AwContentsClient.onCreateWindow callback).
  // Note that if the embedder has blocked support for creating popup
  // windows through AwSettings, then we won't get to this point as
  // the popup creation will have been blocked at the WebKit level.
  if (no_javascript_access) {
    *no_javascript_access = false;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  AwSettings* settings = AwSettings::FromWebContents(web_contents);

  return (settings && settings->GetJavaScriptCanOpenWindowsAutomatically()) ||
         user_gesture;
}

base::FilePath AwContentBrowserClient::GetDefaultDownloadDirectory() {
  // Android WebView does not currently use the Chromium downloads system.
  // Download requests are cancelled immedately when recognized; see
  // AwResourceDispatcherHost::CreateResourceHandlerForDownload. However the
  // download system still tries to start up and calls this before recognizing
  // the request has been cancelled.
  return base::FilePath();
}

std::string AwContentBrowserClient::GetDefaultDownloadName() {
  NOTREACHED() << "Android WebView does not use chromium downloads";
  return std::string();
}

void AwContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  NOTREACHED() << "Android WebView does not support plugins";
}

bool AwContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
  NOTREACHED() << "Android WebView does not support plugins";
  return false;
}

bool AwContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
  NOTREACHED() << "Android WebView does not support plugins";
  return false;
}

content::TracingDelegate* AwContentBrowserClient::GetTracingDelegate() {
  return new AwTracingDelegate();
}

void AwContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  mappings->ShareWithRegion(kAndroidWebViewMainPakDescriptor, fd, region);

  fd = ui::GetCommonResourcesPackFd(&region);
  mappings->ShareWithRegion(kAndroidWebView100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  mappings->ShareWithRegion(kAndroidWebViewLocalePakDescriptor, fd, region);

  int crash_signal_fd =
      crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
  if (crash_signal_fd >= 0) {
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
  }
}

void AwContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  AwSettings* aw_settings = AwSettings::FromWebContents(
      content::WebContents::FromRenderViewHost(rvh));
  if (aw_settings) {
    aw_settings->PopulateWebPreferences(web_prefs);
  }
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
AwContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  // We allow intercepting only navigations within main frames. This
  // is used to post onPageStarted. We handle shouldOverrideUrlLoading
  // via a sync IPC.
  if (navigation_handle->IsInMainFrame()) {
    // MetricsNavigationThrottle requires that it runs before
    // NavigationThrottles that may delay or cancel navigations, so only
    // NavigationThrottles that don't delay or cancel navigations (e.g.
    // throttles that are only observing callbacks without affecting navigation
    // behavior) should be added before MetricsNavigationThrottle.
    throttles.push_back(page_load_metrics::MetricsNavigationThrottle::Create(
        navigation_handle));
    // Use Synchronous mode for the navigation interceptor, since this class
    // doesn't actually call into an arbitrary client, it just posts a task to
    // call onPageStarted. shouldOverrideUrlLoading happens earlier (see
    // ContentBrowserClient::ShouldOverrideUrlLoading).
    throttles.push_back(
        navigation_interception::InterceptNavigationDelegate::CreateThrottleFor(
            navigation_handle, navigation_interception::SynchronyMode::kSync));
    throttles.push_back(std::make_unique<PolicyBlacklistNavigationThrottle>(
        navigation_handle, AwBrowserContext::FromWebContents(
                               navigation_handle->GetWebContents())));
    throttles.push_back(
        std::make_unique<AwSafeBrowsingNavigationThrottle>(navigation_handle));
  }
  return throttles;
}

content::DevToolsManagerDelegate*
AwContentBrowserClient::GetDevToolsManagerDelegate() {
  return new AwDevToolsManagerDelegate();
}

base::Optional<service_manager::Manifest>
AwContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetAWContentBrowserOverlayManifest();
  return base::nullopt;
}

bool AwContentBrowserClient::BindAssociatedReceiverFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  if (interface_name == autofill::mojom::AutofillDriver::Name_) {
    autofill::ContentAutofillDriverFactory::BindAutofillDriver(
        mojo::PendingAssociatedReceiver<autofill::mojom::AutofillDriver>(
            std::move(*handle)),
        render_frame_host);
    return true;
  }
  if (interface_name == content_capture::mojom::ContentCaptureReceiver::Name_) {
    content_capture::ContentCaptureReceiverManager::BindContentCaptureReceiver(
        mojo::PendingAssociatedReceiver<
            content_capture::mojom::ContentCaptureReceiver>(std::move(*handle)),
        render_frame_host);
    return true;
  }

  return false;
}

void AwContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  content::ResourceContext* resource_context =
      render_process_host->GetBrowserContext()->GetResourceContext();
  registry->AddInterface(
      base::BindRepeating(
          &MaybeCreateSafeBrowsing, render_process_host->GetID(),
          resource_context,
          base::BindRepeating(
              &AwContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
              base::Unretained(this))),
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}));

#if BUILDFLAG(ENABLE_SPELLCHECK)
  auto create_spellcheck_host =
      [](mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        mojo::MakeSelfOwnedReceiver(std::make_unique<SpellCheckHostImpl>(),
                                    std::move(receiver));
      };
  registry->AddInterface(
      base::BindRepeating(create_spellcheck_host),
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}));
#endif
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AwContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  result.push_back(safe_browsing::BrowserURLLoaderThrottle::Create(
      base::BindOnce(
          [](AwContentBrowserClient* client) {
            return client->GetSafeBrowsingUrlCheckerDelegate();
          },
          base::Unretained(this)),
      wc_getter, frame_tree_node_id,
      // TODO(crbug.com/1033760): rt_lookup_service is
      // used to perform real time URL check, which is gated by UKM opted-in.
      // Since AW currently doesn't support UKM, this feature is not enabled.
      /* rt_lookup_service */ nullptr));

  if (request.resource_type ==
      static_cast<int>(blink::mojom::ResourceType::kMainFrame)) {
    const bool is_load_url =
        request.transition_type & ui::PAGE_TRANSITION_FROM_API;
    const bool is_go_back_forward =
        request.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK;
    const bool is_reload = ui::PageTransitionCoreTypeIs(
        static_cast<ui::PageTransition>(request.transition_type),
        ui::PAGE_TRANSITION_RELOAD);
    if (is_load_url || is_go_back_forward || is_reload) {
      result.push_back(
          std::make_unique<AwURLLoaderThrottle>(static_cast<AwResourceContext*>(
              browser_context->GetResourceContext())));
    }
  }

  return result;
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
AwContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ = new AwUrlCheckerDelegateImpl(
        AwBrowserProcess::GetInstance()->GetSafeBrowsingDBManager(),
        AwBrowserProcess::GetInstance()->GetSafeBrowsingUIManager(),
        AwBrowserProcess::GetInstance()->GetSafeBrowsingWhitelistManager());
  }

  return safe_browsing_url_checker_delegate_;
}

bool AwContentBrowserClient::ShouldOverrideUrlLoading(
    int frame_tree_node_id,
    bool browser_initiated,
    const GURL& gurl,
    const std::string& request_method,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    ui::PageTransition transition,
    bool* ignore_navigation) {
  *ignore_navigation = false;

  // Only GETs can be overridden.
  if (request_method != "GET")
    return true;

  bool application_initiated =
      browser_initiated || transition & ui::PAGE_TRANSITION_FORWARD_BACK;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return true;

  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  //
  // Note: about:blank navigations are not received in this path at the moment,
  // they use the old SYNC IPC path as they are not handled by network stack.
  // However, the old path should be removed in future.
  if (!is_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return true;

  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents == nullptr)
    return true;
  AwContentsClientBridge* client_bridge =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (client_bridge == nullptr)
    return true;

  base::string16 url = base::UTF8ToUTF16(gurl.possibly_invalid_spec());
  return client_bridge->ShouldOverrideUrlLoading(
      url, has_user_gesture, is_redirect, is_main_frame, ignore_navigation);
}

bool AwContentBrowserClient::ShouldCreateThreadPool() {
  return g_should_create_thread_pool;
}

std::unique_ptr<content::LoginDelegate>
AwContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<AwHttpAuthHandler>(auth_info, web_contents,
                                             first_auth_attempt,
                                             std::move(auth_required_callback));
}

bool AwContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::OnceGetter web_contents_getter,
    int child_id,
    content::NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const base::Optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();
  // We don't need to care for |security_options| as the factories constructed
  // below are used only for navigation.
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    // Manages its own lifetime.
    new android_webview::AwProxyingURLLoaderFactory(
        0 /* process_id */, std::move(receiver), mojo::NullRemote(),
        true /* intercept_only */, base::nullopt /* security_options */);
  } else {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            [](mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                   receiver) {
              // Manages its own lifetime.
              new android_webview::AwProxyingURLLoaderFactory(
                  0 /* process_id */, std::move(receiver), mojo::NullRemote(),
                  true /* intercept_only */,
                  base::nullopt /* security_options */);
            },
            std::move(receiver)));
  }
  return false;
}

void AwContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  WebContents* web_contents = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);

  if (aw_settings && aw_settings->GetAllowFileAccess()) {
    AwBrowserContext* aw_browser_context =
        AwBrowserContext::FromWebContents(web_contents);
    auto file_factory = CreateFileURLLoaderFactory(
        aw_browser_context->GetPath(),
        aw_browser_context->GetSharedCorsOriginAccessList());
    factories->emplace(url::kFileScheme, std::move(file_factory));
  }
}

bool AwContentBrowserClient::ShouldIsolateErrorPage(bool in_main_frame) {
  return false;
}

bool AwContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  // TODO(lukasza): When/if we eventually add OOPIF support for AW we should
  // consider running AW tests with and without site-per-process (and this might
  // require returning true below).  Adding OOPIF support for AW is tracked by
  // https://crbug.com/869494.
  return false;
}

bool AwContentBrowserClient::ShouldLockToOrigin(
    content::BrowserContext* browser_context,
    const GURL& effective_url) {
  // TODO(lukasza): https://crbug.cmo/869494: Once Android WebView supports
  // OOPIFs, we should remove this ShouldLockToOrigin overload.  Till then,
  // returning false helps avoid accidentally applying citadel-style Site
  // Isolation enforcement to Android WebView (and causing incorrect renderer
  // kills).
  return false;
}

bool AwContentBrowserClient::DoesWebUISchemeRequireProcessLock(
    base::StringPiece scheme) {
  // TODO(nasko,alexmos): WebView does not currently lock processes for WebUI
  // navigations, even though it does cross-process navigations. It should be
  // fixed and this method can be removed.
  // See https://crbug.com/1071464.
  return false;
}

bool AwContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    base::Optional<int64_t> navigation_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojo::PendingReceiver<network::mojom::URLLoaderFactory> proxied_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;

  if (factory_override) {
    // We are interested in factories "inside" of CORS, so use
    // |factory_override|.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New();
    proxied_receiver =
        (*factory_override)
            ->overriding_factory.InitWithNewPipeAndPassReceiver();
    (*factory_override)->overridden_factory_receiver =
        target_factory_remote.InitWithNewPipeAndPassReceiver();
    (*factory_override)->skip_cors_enabled_scheme_check = true;
  } else {
    // In this case, |factory_override| is not given. But all callers of
    // ContentBrowserClient::WillCreateURLLoaderFactory guarantee that
    // |factory_override| is null only when the security features on the network
    // service is no-op for requests coming to the URLLoaderFactory. Hence we
    // can use |factory_receiver| here.
    proxied_receiver = std::move(*factory_receiver);
    *factory_receiver = target_factory_remote.InitWithNewPipeAndPassReceiver();
  }
  int process_id =
      type == URLLoaderFactoryType::kNavigation ? 0 : render_process_id;

  // Android WebView has one non off-the-record browser context.
  if (frame) {
    auto security_options =
        base::make_optional<AwProxyingURLLoaderFactory::SecurityOptions>();
    security_options->disable_web_security =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableWebSecurity);
    const auto& preferences =
        frame->GetRenderViewHost()->GetWebkitPreferences();
    // See also //android_webview/docs/cors-and-webview-api.md to understand how
    // each settings affect CORS behaviors on file:// and content://.
    if (request_initiator.scheme() == url::kFileScheme) {
      security_options->disable_web_security |=
          preferences.allow_universal_access_from_file_urls;
      // Usual file:// to file:// requests are mapped to kNoCors if the setting
      // is set to true. Howover, file:///android_{asset|res}/ still uses kCors
      // and needs to permit it in the |security_options|.
      security_options->allow_cors_to_same_scheme =
          preferences.allow_file_access_from_file_urls;
    } else if (request_initiator.scheme() == url::kContentScheme) {
      security_options->allow_cors_to_same_scheme =
          preferences.allow_file_access_from_file_urls ||
          preferences.allow_universal_access_from_file_urls;
    }
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&AwProxyingURLLoaderFactory::CreateProxy, process_id,
                       std::move(proxied_receiver),
                       std::move(target_factory_remote), security_options));
  } else {
    // A service worker and worker subresources set nullptr to |frame|, and
    // work without seeing the AllowUniversalAccessFromFileURLs setting. So,
    // we don't pass a valid |security_options| here.
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindOnce(&AwProxyingURLLoaderFactory::CreateProxy,
                                  process_id, std::move(proxied_receiver),
                                  std::move(target_factory_remote),
                                  base::nullopt /* security_options */));
  }
  return true;
}

uint32_t AwContentBrowserClient::GetWebSocketOptions(
    content::RenderFrameHost* frame) {
  uint32_t options = network::mojom::kWebSocketOptionNone;
  if (!frame) {
    return options;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);

  bool global_cookie_policy =
      AwCookieAccessPolicy::GetInstance()->GetShouldAcceptCookies();
  bool third_party_cookie_policy = aw_contents->AllowThirdPartyCookies();
  if (!global_cookie_policy) {
    options |= network::mojom::kWebSocketOptionBlockAllCookies;
  } else if (!third_party_cookie_policy) {
    options |= network::mojom::kWebSocketOptionBlockThirdPartyCookies;
  }
  return options;
}

bool AwContentBrowserClient::WillCreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool is_service_worker,
    int process_id,
    int routing_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver) {
  mojo::PendingReceiver<network::mojom::RestrictedCookieManager> orig_receiver =
      std::move(*receiver);

  mojo::PendingRemote<network::mojom::RestrictedCookieManager>
      target_rcm_remote;
  *receiver = target_rcm_remote.InitWithNewPipeAndPassReceiver();

  AwProxyingRestrictedCookieManager::CreateAndBind(
      std::move(target_rcm_remote), is_service_worker, process_id, routing_id,
      std::move(orig_receiver));

  return false;  // only made a proxy, still need the actual impl to be made.
}

std::string AwContentBrowserClient::GetProduct() {
  return android_webview::GetProduct();
}

std::string AwContentBrowserClient::GetUserAgent() {
  return android_webview::GetUserAgent();
}

content::ContentBrowserClient::WideColorGamutHeuristic
AwContentBrowserClient::GetWideColorGamutHeuristic() {
  if (base::FeatureList::IsEnabled(features::kWebViewWideColorGamutSupport))
    return WideColorGamutHeuristic::kUseWindow;

  if (display::Display::HasForceDisplayColorProfile() &&
      display::Display::GetForcedDisplayColorProfile() ==
          gfx::ColorSpace::CreateDisplayP3D65()) {
    return WideColorGamutHeuristic::kUseWindow;
  }

  return WideColorGamutHeuristic::kNone;
}

void AwContentBrowserClient::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::mojom::PageLoadFeatures new_features({feature}, {}, {});
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, new_features);
}

bool AwContentBrowserClient::IsOriginTrialRequiredForAppCache(
    content::BrowserContext* browser_text) {
  // WebView has no way of specifying an origin trial, and so never
  // consider it a requirement.
  return false;
}

content::SpeechRecognitionManagerDelegate*
AwContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new AwSpeechRecognitionManagerDelegate();
}

// static
void AwContentBrowserClient::DisableCreatingThreadPool() {
  g_should_create_thread_pool = false;
}

}  // namespace android_webview
