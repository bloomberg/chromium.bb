// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "chrome/renderer/media/chrome_key_systems_provider.h"
#include "components/nacl/common/buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "chrome/common/conflicts/remote_module_watcher_win.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/common/plugin.mojom.h"
#endif

class ChromeRenderThreadObserver;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class ChromePDFPrintClient;
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif
class ThreadProfiler;

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace chrome {
class WebRtcLoggingAgentImpl;
}  // namespace chrome

namespace content {
class BrowserPluginDelegate;
struct WebPluginInfo;
}  // namespace content

namespace extensions {
class Extension;
}

namespace subresource_filter {
class UnverifiedRulesetDealer;
}

namespace web_cache {
class WebCacheImpl;
}

class ChromeContentRendererClient
    : public content::ContentRendererClient,
      public service_manager::LocalInterfaceProvider {
 public:
  ChromeContentRendererClient();
  ~ChromeContentRendererClient() override;

  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderViewCreated(content::RenderView* render_view) override;
  SkBitmap* GetSadPluginBitmap() override;
  SkBitmap* GetSadWebViewBitmap() override;
  bool IsPluginHandledExternally(content::RenderFrame* render_frame,
                                 const blink::WebElement& plugin_element,
                                 const GURL& original_url,
                                 const std::string& mime_type) override;
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate) override;
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            blink::WebPlugin** plugin) override;
  blink::WebPlugin* CreatePluginReplacement(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path) override;
  bool HasErrorPage(int http_status_code) override;
  bool ShouldSuppressErrorPage(content::RenderFrame* render_frame,
                               const GURL& url,
                               int error_code) override;
  bool ShouldTrackUseCounter(const GURL& url) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        std::string* error_html) override;
  void PrepareErrorPageForHttpStatusError(content::RenderFrame* render_frame,
                                          const GURL& unreachable_url,
                                          const std::string& http_method,
                                          int http_status,
                                          std::string* error_html) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) override;
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* compositor_thread_task_runner) override;
  bool RunIdleHandlerWhenWidgetsHidden() override;
  bool AllowPopup() override;
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url,
                       bool* force_ignore_site_for_cookies) override;
  bool IsPrefetchOnly(content::RenderFrame* render_frame,
                      const blink::WebURLRequest& request) override;
  uint64_t VisitedLinkHash(const char* canonical_url, size_t length) override;
  bool IsLinkVisited(uint64_t link_hash) override;
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      content::RenderFrame* render_frame) override;
  bool IsExternalPepperPlugin(const std::string& module_name) override;
  bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path) override;
  std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  bool ShouldReportDetailedMessageForSource(
      const base::string16& source) override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient(
      content::RenderFrame* render_frame) override;
#if !defined(OS_ANDROID)
  std::unique_ptr<media::SpeechRecognitionClient> CreateSpeechRecognitionClient(
      content::RenderFrame* render_frame) override;
#endif
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems)
      override;
  bool IsKeySystemsUpdateNeeded() override;
  bool IsPluginAllowedToUseDevChannelAPIs() override;
  bool IsPluginAllowedToUseCameraDeviceAPI(const GURL& url) override;
  content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const content::WebPluginInfo& info,
      const std::string& mime_type,
      const GURL& original_url) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  void WillInitializeServiceWorkerContextOnWorkerThread() override;
  void DidInitializeServiceWorkerContextOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  void WillEvaluateServiceWorkerOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      v8::Local<v8::Context> v8_context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  void DidStartServiceWorkerContextOnWorkerThread(
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  bool IsExcludedHeaderForServiceWorkerFetchEvent(
      const std::string& header_name) override;
  bool ShouldEnforceWebRTCRoutingPreferences() override;
  GURL OverrideFlashEmbedWithHTML(const GURL& url) override;
  std::unique_ptr<content::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType provider_type) override;
  blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                             const std::string& name) override;
  bool IsSafeRedirectTarget(const GURL& url) override;
  void DidSetUserAgent(const std::string& user_agent) override;
  bool RequiresWebComponentsV0(const GURL& url) override;

#if BUILDFLAG(ENABLE_PLUGINS)
  static mojo::AssociatedRemote<chrome::mojom::PluginInfoHost>&
  GetPluginInfoHost();

  static blink::WebPlugin* CreatePlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params,
      const chrome::mojom::PluginInfo& plugin_info);
#endif

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  static bool IsExtensionOrSharedModuleWhitelisted(
      const GURL& url,
      const std::set<std::string>& whitelist);
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
  void InitSpellCheck();
#endif

  ChromeRenderThreadObserver* GetChromeObserver() const;
  web_cache::WebCacheImpl* GetWebCache();
  chrome::WebRtcLoggingAgentImpl* GetWebRtcLoggingAgent();

#if BUILDFLAG(ENABLE_SPELLCHECK)
  SpellCheck* GetSpellCheck();
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest, NaClRestriction);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest,
                           ShouldSuppressErrorPage);

  static GURL GetNaClContentHandlerURL(const std::string& actual_mime_type,
                                       const content::WebPluginInfo& plugin);

  // service_manager::LocalInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  void BindWebRTCLoggingAgent(
      mojo::PendingReceiver<chrome::mojom::WebRtcLoggingAgent> receiver);

#if BUILDFLAG(ENABLE_NACL)
  // Determines if a page/app/extension is allowed to run native (non-PNaCl)
  // NaCl modules.
  static bool IsNativeNaClAllowed(const GURL& app_url,
                                  bool is_nacl_unrestricted,
                                  const extensions::Extension* extension);
  static void ReportNaClAppType(bool is_pnacl,
                                bool is_extension_or_app,
                                bool is_hosted_app);
#endif

#if defined(OS_WIN)
  // Observes module load events and notifies the ModuleDatabase in the browser
  // process. This instance is created on the main thread but then lives on the
  // IO task runner.
  RemoteModuleWatcher::UniquePtr remote_module_watcher_;
#endif

  // Used to profile main thread.
  std::unique_ptr<ThreadProfiler> main_thread_profiler_;

  std::unique_ptr<ChromeRenderThreadObserver> chrome_observer_;
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
  std::unique_ptr<chrome::WebRtcLoggingAgentImpl> webrtc_logging_agent_impl_;

  ChromeKeySystemsProvider key_systems_provider_;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::unique_ptr<SpellCheck> spellcheck_;
#endif
  std::unique_ptr<subresource_filter::UnverifiedRulesetDealer>
      subresource_filter_ruleset_dealer_;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  std::unique_ptr<ChromePDFPrintClient> pdf_print_client_;
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  std::set<std::string> allowed_camera_device_origins_;
#endif

  scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentRendererClient);
};

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
