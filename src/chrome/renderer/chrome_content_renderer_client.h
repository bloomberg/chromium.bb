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
#include "base/strings/string16.h"
#include "chrome/common/plugin.mojom.h"
#include "chrome/renderer/media/chrome_key_systems_provider.h"
#include "components/nacl/common/buildflags.h"
#include "components/rappor/public/interfaces/rappor_recorder.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#endif

class ChromeRenderThreadObserver;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class ChromePDFPrintClient;
#endif
class PrescientNetworkingDispatcher;
#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif
class ThreadProfiler;

namespace content {
class BrowserPluginDelegate;
struct WebPluginInfo;
}

namespace network_hints {
class PrescientNetworkingDispatcher;
}

namespace extensions {
class Extension;
}

namespace prerender {
class PrerenderDispatcher;
}

namespace subresource_filter {
class UnverifiedRulesetDealer;
}

namespace web_cache {
class WebCacheImpl;
}

class WebRtcLoggingMessageFilter;

class ChromeContentRendererClient
    : public content::ContentRendererClient,
      public service_manager::Service,
      public service_manager::LocalInterfaceProvider {
 public:
  ChromeContentRendererClient();
  ~ChromeContentRendererClient() override;

  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderViewCreated(content::RenderView* render_view) override;
  SkBitmap* GetSadPluginBitmap() override;
  SkBitmap* GetSadWebViewBitmap() override;
  bool MaybeCreateMimeHandlerView(content::RenderFrame* render_frame,
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
                               const GURL& url) override;
  bool ShouldTrackUseCounter(const GURL& url) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        bool ignoring_cache,
                        std::string* error_html) override;
  void PrepareErrorPageForHttpStatusError(content::RenderFrame* render_frame,
                                          const GURL& unreachable_url,
                                          const std::string& http_method,
                                          bool ignoring_cache,
                                          int http_status,
                                          std::string* error_html) override;

  void GetErrorDescription(const blink::WebURLError& error,
                           const std::string& http_method,
                           base::string16* error_description) override;

  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) override;
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* compositor_thread_task_runner) override;
  bool RunIdleHandlerWhenWidgetsHidden() override;
  bool AllowPopup() override;
  bool ShouldFork(blink::WebLocalFrame* frame,
                  const GURL& url,
                  const std::string& http_method,
                  bool is_initial_navigation,
                  bool is_server_redirect) override;
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const url::Origin* initiator_origin,
                       GURL* new_url,
                       bool* attach_same_site_cookies) override;
  bool IsPrefetchOnly(content::RenderFrame* render_frame,
                      const blink::WebURLRequest& request) override;
  unsigned long long VisitedLinkHash(const char* canonical_url,
                                     size_t length) override;
  bool IsLinkVisited(unsigned long long link_hash) override;
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  bool IsPrerenderingFrame(const content::RenderFrame* render_frame) override;
  bool IsExternalPepperPlugin(const std::string& module_name) override;
  bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path) override;
  std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  std::unique_ptr<blink::WebSpeechSynthesizer> OverrideSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client) override;
  bool ShouldReportDetailedMessageForSource(
      const base::string16& source) override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient(
      content::RenderFrame* render_frame) override;
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
  void RecordRappor(const std::string& metric,
                    const std::string& sample) override;
  void RecordRapporURL(const std::string& metric, const GURL& url) override;
  void AddImageContextMenuProperties(
      const blink::WebURLResponse& response,
      bool is_image_in_context_a_placeholder_image,
      std::map<std::string, std::string>* properties) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  void DidInitializeServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
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
  std::unique_ptr<base::TaskScheduler::InitParams> GetTaskSchedulerInitParams()
      override;
  void CreateRendererService(
      service_manager::mojom::ServiceRequest service_request) override;
  std::unique_ptr<content::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType provider_type) override;
  blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                             const std::string& name) override;
  bool IsSafeRedirectTarget(const GURL& url) override;
  void DidSetUserAgent(const std::string& user_agent) override;

#if BUILDFLAG(ENABLE_PLUGINS)
  static chrome::mojom::PluginInfoHostAssociatedPtr& GetPluginInfoHost();

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

  prerender::PrerenderDispatcher* prerender_dispatcher() const {
    return prerender_dispatcher_.get();
  }

  base::WeakPtr<ChromeRenderThreadObserver> GetChromeObserver() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest, NaClRestriction);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest,
                           ShouldSuppressErrorPage);

  static GURL GetNaClContentHandlerURL(const std::string& actual_mime_type,
                                       const content::WebPluginInfo& plugin);

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& name,
                       mojo::ScopedMessagePipeHandle handle) override;

  // service_manager::LocalInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  // Time at which this object was created. This is very close to the time at
  // which the RendererMain function was entered.
  base::TimeTicks main_entry_time_;

#if BUILDFLAG(ENABLE_NACL)
  // Determines if a NaCl app is allowed, and modifies params to pass the app's
  // permissions to the trusted NaCl plugin.
  static bool IsNaClAllowed(const GURL& app_url,
                            bool is_nacl_unrestricted,
                            const extensions::Extension* extension,
                            blink::WebPluginParams* params);
#endif

  service_manager::Connector* GetConnector();

  // Used to profile main thread.
  std::unique_ptr<ThreadProfiler> main_thread_profiler_;

  rappor::mojom::RapporRecorderPtr rappor_recorder_;

  std::unique_ptr<ChromeRenderThreadObserver> chrome_observer_;
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;

  std::unique_ptr<network_hints::PrescientNetworkingDispatcher>
      prescient_networking_dispatcher_;

  ChromeKeySystemsProvider key_systems_provider_;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::unique_ptr<SpellCheck> spellcheck_;
#endif
  std::unique_ptr<subresource_filter::UnverifiedRulesetDealer>
      subresource_filter_ruleset_dealer_;
  std::unique_ptr<prerender::PrerenderDispatcher> prerender_dispatcher_;
  scoped_refptr<WebRtcLoggingMessageFilter> webrtc_logging_message_filter_;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  std::unique_ptr<ChromePDFPrintClient> pdf_print_client_;
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  std::set<std::string> allowed_camera_device_origins_;
#endif

#if defined(OS_WIN)
  // Observes module load and unload events and notifies the ModuleDatabase in
  // the browser process.
  std::unique_ptr<ModuleWatcher> module_watcher_;
  mojom::ModuleEventSinkPtr module_event_sink_;
#endif

  service_manager::ServiceBinding service_binding_{this};
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentRendererClient);
};

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
