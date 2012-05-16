// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/content_client/examples_content_browser_client.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/shell/shell.h"
#include "content/shell/shell_devtools_delegate.h"
#include "content/shell/shell_render_view_host_observer.h"
#include "content/shell/shell_resource_dispatcher_host_delegate.h"
#include "content/shell/shell_switches.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/examples/content_client/examples_browser_main_parts.h"

namespace views {
namespace examples {

ExamplesContentBrowserClient::ExamplesContentBrowserClient()
    : examples_browser_main_parts_(NULL) {
}

ExamplesContentBrowserClient::~ExamplesContentBrowserClient() {
}

content::BrowserMainParts* ExamplesContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  examples_browser_main_parts_ =  new ExamplesBrowserMainParts(parameters);
  return examples_browser_main_parts_;
}

content::WebContentsView*
    ExamplesContentBrowserClient::OverrideCreateWebContentsView(
        content::WebContents* web_contents) {
  return NULL;
}

content::WebContentsViewDelegate*
    ExamplesContentBrowserClient::GetWebContentsViewDelegate(
        content::WebContents* web_contents) {
  return NULL;
}

void ExamplesContentBrowserClient::RenderViewHostCreated(
    content::RenderViewHost* render_view_host) {
  new content::ShellRenderViewHostObserver(render_view_host);
}

void ExamplesContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
}

content::WebUIControllerFactory*
    ExamplesContentBrowserClient::GetWebUIControllerFactory() {
  return NULL;
}

GURL ExamplesContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  return GURL();
}

bool ExamplesContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool ExamplesContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool ExamplesContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  return true;
}

bool ExamplesContentBrowserClient::ShouldTryToUseExistingProcessHost(
    content::BrowserContext* browser_context, const GURL& url) {
  return false;
}

void ExamplesContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
}

void ExamplesContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
}

bool ExamplesContentBrowserClient::ShouldSwapProcessesForNavigation(
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

std::string ExamplesContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return std::string();
}

void ExamplesContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    command_line->AppendSwitch(switches::kDumpRenderTree);
}

std::string ExamplesContentBrowserClient::GetApplicationLocale() {
  return std::string();
}

std::string ExamplesContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return std::string();
}

SkBitmap* ExamplesContentBrowserClient::GetDefaultFavicon() {
  static SkBitmap empty;
  return &empty;
}

bool ExamplesContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& first_party,
    content::ResourceContext* context) {
  return true;
}

bool ExamplesContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    content::ResourceContext* context,
    int render_process_id,
    int render_view_id) {
  return true;
}

bool ExamplesContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    content::ResourceContext* context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  return true;
}

bool ExamplesContentBrowserClient::AllowSaveLocalState(
    content::ResourceContext* context) {
  return true;
}

bool ExamplesContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool ExamplesContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool ExamplesContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

content::QuotaPermissionContext*
    ExamplesContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

net::URLRequestContext*
    ExamplesContentBrowserClient::OverrideRequestContextForURL(
        const GURL& url, content::ResourceContext* context) {
  return NULL;
}

void ExamplesContentBrowserClient::OpenItem(const FilePath& path) {
}

void ExamplesContentBrowserClient::ShowItemInFolder(const FilePath& path) {
}

void ExamplesContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    bool strict_enforcement,
    const base::Callback<void(bool)>& callback,
    bool* cancel_request) {
}

void ExamplesContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
}

void ExamplesContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
}

void ExamplesContentBrowserClient::RequestMediaAccessPermission(
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback) {
}

content::MediaObserver* ExamplesContentBrowserClient::GetMediaObserver() {
  return NULL;
}

void ExamplesContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
}

WebKit::WebNotificationPresenter::Permission
    ExamplesContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_origin,
        content::ResourceContext* context,
        int render_process_id) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void ExamplesContentBrowserClient::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
}

void ExamplesContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
}

bool ExamplesContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& origin,
    WindowContainerType container_type,
    content::ResourceContext* context,
    int render_process_id,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

std::string ExamplesContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, content::ResourceContext* context) {
  return std::string();
}

void ExamplesContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new content::ShellResourceDispatcherHostDelegate);
  content::ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

content::SpeechRecognitionManagerDelegate*
    ExamplesContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
  return NULL;
}

ui::Clipboard* ExamplesContentBrowserClient::GetClipboard() {
  return examples_browser_main_parts_->GetClipboard();
}

net::NetLog* ExamplesContentBrowserClient::GetNetLog() {
  return NULL;
}

content::AccessTokenStore*
    ExamplesContentBrowserClient::CreateAccessTokenStore() {
  return NULL;
}

bool ExamplesContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

void ExamplesContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    const GURL& url,
    webkit_glue::WebPreferences* prefs) {
}

void ExamplesContentBrowserClient::UpdateInspectorSetting(
    content::RenderViewHost* rvh,
    const std::string& key,
    const std::string& value) {
}

void ExamplesContentBrowserClient::ClearInspectorSettings(
    content::RenderViewHost* rvh) {
}

void ExamplesContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
}

void ExamplesContentBrowserClient::ClearCache(content::RenderViewHost* rvh) {
}

void ExamplesContentBrowserClient::ClearCookies(content::RenderViewHost* rvh) {
}

FilePath ExamplesContentBrowserClient::GetDefaultDownloadDirectory() {
  return FilePath();
}

std::string ExamplesContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

bool ExamplesContentBrowserClient::AllowSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int ExamplesContentBrowserClient::GetCrashSignalFD(
    const CommandLine& command_line) {
  return -1;
}
#endif

#if defined(OS_WIN)
const wchar_t* ExamplesContentBrowserClient::GetResourceDllName() {
  return NULL;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ExamplesContentBrowserClient::GetCryptoPasswordDelegate(const GURL& url) {
  return NULL;
}
#endif

content::ShellBrowserContext* ExamplesContentBrowserClient::browser_context() {
  return examples_browser_main_parts_->browser_context();
}

}  // namespace examples
}  // namespace views
