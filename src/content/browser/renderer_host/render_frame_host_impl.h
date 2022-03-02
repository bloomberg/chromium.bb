// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/strings/string_piece.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/can_commit_status.h"
#include "content/browser/net/cross_origin_opener_policy_reporter.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/browsing_context_state.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/cross_origin_opener_policy_access_report_manager.h"
#include "content/browser/renderer_host/keep_alive_handle_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/dom_automation_controller.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/common/input/input_injector.mojom-forward.h"
#include "content/common/navigation_client.mojom-forward.h"
#include "content/common/navigation_client.mojom.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/javascript_dialog_type.h"
#include "media/mojo/mojom/interface_factory.mojom-forward.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-forward.h"
#include "media/mojo/mojom/media_player.mojom-forward.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/net_buildflags.h"
#include "services/device/public/mojom/sensor_provider.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom-forward.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom-forward.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-forward.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-forward.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-forward.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-forward.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom-forward.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_response.mojom-forward.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/containers/id_map.h"
#include "services/device/public/mojom/nfc.mojom.h"
#else
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#endif

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "media/mojo/mojom/remoting.mojom-forward.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_plugin.mojom.h"
#endif

namespace blink {
class AssociatedInterfaceRegistry;
class DocumentPolicy;
struct FramePolicy;
struct TransferableMessage;
struct UntrustworthyContextMenuParams;

namespace mojom {
class CacheStorage;
class DeviceAPIService;
class GeolocationService;
class ManagedConfigurationService;
class WebUsbService;
}  // namespace mojom
}  // namespace blink

namespace gfx {
class Range;
}

namespace mojo {
class MessageFilter;
}

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace ui {
class ClipboardFormatType;
}

namespace ukm {
class UkmRecorder;
}

namespace content {

namespace internal {
class DocumentServiceBase;
}  // namespace internal

class AgentSchedulingGroupHost;
class BrowsingContextState;
class CodeCacheHostImpl;
class CrossOriginEmbedderPolicyReporter;
class CrossOriginOpenerPolicyAccessReportManager;
class FeatureObserver;
class FencedFrame;
class FrameTree;
class FrameTreeNode;
class GeolocationServiceImpl;
class IdleManagerImpl;
class NavigationEarlyHintsManager;
class NavigationRequest;
class PeakGpuMemoryTracker;
class PeerConnectionTrackerHost;
class PepperPluginInstanceHost;
class Portal;
class PrefetchedSignedExchangeCache;
class PresentationServiceImpl;
class PushMessagingManager;
class RenderAccessibilityHost;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
class RenderFrameHostOrProxy;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class SensorProviderProxyImpl;
class ServiceWorkerContainerHost;
class SiteInfo;
class SpeechSynthesisImpl;
class SubresourceWebBundleNavigationInfo;
class TimeoutMonitor;
class WebAuthRequestSecurityChecker;
class WebBluetoothServiceImpl;
class WebBundleHandle;
class WebBundleHandleTracker;
class WebUIImpl;
struct PendingNavigation;
struct ResourceTimingInfo;
struct SubresourceLoaderParams;

// To be called when a RenderFrameHostImpl receives an event.
// Provides the host, the event fired, and which node id the event was for.
typedef base::RepeatingCallback<
    void(RenderFrameHostImpl*, ax::mojom::Event, int)>
    AccessibilityCallbackForTesting;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public base::SupportsUserData,
      public mojom::FrameHost,
      public mojom::DomAutomationControllerHost,
      public BrowserAccessibilityDelegate,
      public RenderProcessHostObserver,
      public SiteInstanceGroup::Observer,
      public blink::mojom::BackForwardCacheControllerHost,
      public blink::mojom::LocalFrameHost,
      public blink::mojom::LocalMainFrameHost,
      public ui::AXActionHandlerBase,
#if BUILDFLAG(ENABLE_PLUGINS)
      public mojom::PepperHost,
      public mojom::PepperHungDetectorHost,
#endif  //  BUILDFLAG(ENABLE_PLUGINS)
      public network::mojom::CookieAccessObserver {
 public:
  using JavaScriptDialogCallback =
      content::JavaScriptDialogManager::DialogClosedCallback;

  // Callback used with IsClipboardPasteContentAllowed() method.
  using ClipboardPasteContentAllowed =
      ContentBrowserClient::ClipboardPasteContentAllowed;
  using IsClipboardPasteContentAllowedCallback =
      ContentBrowserClient::IsClipboardPasteContentAllowedCallback;

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  // Can be set to 0 to fail immediately during tests.
  static int max_accessibility_resets_;

  static RenderFrameHostImpl* FromID(GlobalRenderFrameHostId id);
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  static RenderFrameHostImpl* FromFrameToken(
      int process_id,
      const blink::LocalFrameToken& frame_token,
      mojo::ReportBadMessageCallback* process_mismatch_callback = nullptr);

  static RenderFrameHostImpl* FromAXTreeID(ui::AXTreeID ax_tree_id);
  static RenderFrameHostImpl* FromOverlayRoutingToken(
      const base::UnguessableToken& token);

  // Clears the all prefetched cached signed exchanges.
  static void ClearAllPrefetchedSignedExchangeCache();

  // TODO(crbug.com/1213818): Get/SetCodeCacheHostReceiverHandler are used only
  // for a test in content/browser/service_worker/service_worker_browsertest
  // that tests a bad message is returned on an incorrect origin. Try to find a
  // way to test this without adding these additional methods.
  // Allows external code to supply a callback that is invoked immediately
  // after the CodeCacheHostImpl is created and bound.  Used for swapping
  // the binding for a test version of the service.
  using CodeCacheHostReceiverHandler = base::RepeatingCallback<void(
      CodeCacheHostImpl*,
      mojo::ReceiverId,
      mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>&)>;
  static void SetCodeCacheHostReceiverHandlerForTesting(
      CodeCacheHostReceiverHandler handler);

  RenderFrameHostImpl(const RenderFrameHostImpl&) = delete;
  RenderFrameHostImpl& operator=(const RenderFrameHostImpl&) = delete;

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  const blink::LocalFrameToken& GetFrameToken() override;
  const base::UnguessableToken& GetReportingSource() override;

  ui::AXTreeID GetAXTreeID() override;
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             const ui::AXMode& ax_mode,
                             bool exclude_offscreen,
                             size_t max_nodes,
                             const base::TimeDelta& timeout) override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  GlobalRenderFrameHostId GetGlobalId() override;
  RenderWidgetHostImpl* GetRenderWidgetHost() override;
  RenderWidgetHostView* GetView() override;
  RenderFrameHostImpl* GetParent() override;
  RenderFrameHostImpl* GetParentOrOuterDocument() override;
  RenderFrameHostImpl* GetMainFrame() override;
  PageImpl& GetPage() override;
  bool IsInPrimaryMainFrame() override;
  RenderFrameHostImpl* GetOutermostMainFrame() override;
  bool IsFencedFrameRoot() override;
  bool IsNestedWithinFencedFrame() override;
  void ForEachRenderFrameHost(FrameIterationCallback on_frame) override;
  void ForEachRenderFrameHost(
      FrameIterationAlwaysContinueCallback on_frame) override;
  // TODO (crbug.com/1251545) : Frame tree node id should only be known for
  // subframes. As such, update this method.
  int GetFrameTreeNodeId() const override;
  const base::UnguessableToken& GetDevToolsFrameToken() override;
  absl::optional<base::UnguessableToken> GetEmbeddingToken() override;
  const std::string& GetFrameName() override;
  bool IsFrameDisplayNone() override;
  const absl::optional<gfx::Size>& GetFrameSize() override;
  size_t GetFrameDepth() override;
  bool IsCrossProcessSubframe() override;
  WebExposedIsolationLevel GetWebExposedIsolationLevel() override;
  const GURL& GetLastCommittedURL() override;
  const url::Origin& GetLastCommittedOrigin() override;
  const net::NetworkIsolationKey& GetNetworkIsolationKey() override;
  const net::IsolationInfo& GetIsolationInfoForSubresources() override;
  net::IsolationInfo GetPendingIsolationInfoForSubresources() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScriptMethod(const std::u16string& object_name,
                               const std::u16string& method_name,
                               base::Value arguments,
                               JavaScriptResultCallback callback) override;
  void ExecuteJavaScript(const std::u16string& javascript,
                         JavaScriptResultCallback callback) override;
  void ExecuteJavaScriptInIsolatedWorld(const std::u16string& javascript,
                                        JavaScriptResultCallback callback,
                                        int32_t world_id) override;
  void ExecuteJavaScriptForTests(
      const std::u16string& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const std::u16string& javascript,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) override;
  void ExecutePluginActionAtLocalLocation(
      const gfx::Point& local_location,
      blink::mojom::PluginActionType plugin_action) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(VisualStateCallback callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  content::PageVisibilityState GetVisibilityState() override;
  bool IsRenderFrameCreated() override;
  bool IsRenderFrameLive() override;
  LifecycleState GetLifecycleState() override;
  bool IsInLifecycleState(LifecycleState lifecycle_state) override;
  bool IsActive() override;
  bool IsInactiveAndDisallowActivation(uint64_t reason) override;
  size_t GetProxyCount() override;
  bool HasSelection() override;
  const net::HttpResponseHeaders* GetLastResponseHeaders() override;
  void RequestTextSurroundingSelection(
      blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
      int max_length) override;
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override;
  WebUI* GetWebUI() override;
  void AllowBindings(int binding_flags) override;
  int GetEnabledBindings() override;
  void SetWebUIProperty(const std::string& name,
                        const std::string& value) override;
  void DisableBeforeUnloadHangMonitorForTesting() override;
  bool IsBeforeUnloadHangMonitorDisabledForTesting() override;
  bool GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  bool IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature feature) override;
  void ViewSource() override;
  void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point&,
      const blink::mojom::MediaPlayerAction& action) override;
  bool CreateNetworkServiceDefaultFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver) override;
  void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      const base::flat_set<url::Origin>& isolated_world_origins,
      bool push_to_renderer_now) override;
  bool IsSandboxed(network::mojom::WebSandboxFlags flags) override;
  void FlushNetworkAndNavigationInterfacesForTesting(
      bool do_nothing_if_no_network_service_connection = false) override;
  void PrepareForInnerWebContentsAttach(
      PrepareForInnerWebContentsAttachCallback callback) override;
  blink::FrameOwnerElementType GetFrameOwnerElementType() override;
  bool HasTransientUserActivation() override;
  void NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type) override;
  bool Reload() override;
  bool IsDOMContentLoaded() override;
  void UpdateIsAdSubframe(bool is_ad_subframe) override;
  std::pair<blink::mojom::AuthenticatorStatus, bool>
  PerformGetAssertionWebAuthSecurityChecks(
      const std::string& relying_party_id,
      const url::Origin& effective_origin,
      bool is_payment_credential_get_assertion) override;
  blink::mojom::AuthenticatorStatus PerformMakeCredentialWebAuthSecurityChecks(
      const std::string& relying_party_id,
      const url::Origin& effective_origin,
      bool is_payment_credential_creation) override;
  void SetIsXrOverlaySetup() override;
  ukm::SourceId GetPageUkmSourceId() override;
  StoragePartitionImpl* GetStoragePartition() override;
  BrowserContext* GetBrowserContext() override;
  void ReportInspectorIssue(blink::mojom::InspectorIssueInfoPtr info) override;
  void WriteIntoTrace(perfetto::TracedValue context) override;
  void GetCanonicalUrl(
      base::OnceCallback<void(const absl::optional<GURL>&)> callback) override;
  bool IsErrorDocument() override;
  DocumentRef GetDocumentRef() override;
  WeakDocumentPtr GetWeakDocumentPtr() override;

  // Additional non-override const version of GetMainFrame.
  const RenderFrameHostImpl* GetMainFrame() const;

  // Additional non-override const version of GetParent.
  const RenderFrameHostImpl* GetParent() const;

  // Write a representation of this object into a trace.
  void WriteIntoTrace(
      perfetto::TracedProto<perfetto::protos::pbzero::RenderFrameHost> proto);

  // Determines if a clipboard paste using |data| of type |data_type| is allowed
  // in this renderer frame.  The implementation delegates to
  // RenderFrameHostDelegate::IsClipboardPasteContentAllowed().  See the
  // description of the latter method for complete details.
  void IsClipboardPasteContentAllowed(
      const ui::ClipboardFormatType& data_type,
      const std::string& data,
      IsClipboardPasteContentAllowedCallback callback);

  void SendAccessibilityEventsToManager(
      const AXEventNotificationDetails& details);

  // This is called when accessibility events arrive from renderer to browser.
  // This could cause eviction if the page is in back/forward cache. Returns
  // true if the eviction happens, and otherwise calls
  // |RenderFrameHost::IsInactiveAndDisallowActivation()| and returns the value
  // from there.
  bool IsInactiveAndDisallowActivationForAXEvents(
      const std::vector<ui::AXEvent>& events);

  void EvictFromBackForwardCacheWithReason(
      BackForwardCacheMetrics::NotRestoredReason reason);
  void EvictFromBackForwardCacheWithReasons(
      const BackForwardCacheCanStoreDocumentResult& can_store_flat,
      std::unique_ptr<BackForwardCacheCanStoreTreeResult> can_store_tree =
          nullptr);

  // Only for testing sticky WebBackForwardCacheDisablingFeature.
  void UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  // Returns the current WebPreferences for the WebContents associated with this
  // RenderFrameHost. Will create one if it does not exist (and update all the
  // renderers with the newly computed value).
  blink::web_pref::WebPreferences GetOrCreateWebPreferences();

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;
  std::string ToDebugString() override;

  // BrowserAccessibilityDelegate
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  RenderFrameHostImpl* AccessibilityRenderFrameHost() override;
  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      ax::mojom::Event opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                              int hit_node_id)> opt_callback) override;
  bool AccessibilityIsMainFrame() override;
  WebContentsAccessibility* AccessibilityGetWebContentsAccessibility() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // SiteInstanceGroup::Observer
  void RenderProcessGone(SiteInstanceGroup* site_instance_group,
                         const ChildProcessTerminationInfo& info) override;

  // ui::AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& data) override;
  bool RequiresPerformActionPointInPixels() const override;

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(
      int previous_routing_id,
      const absl::optional<blink::FrameToken>& opener_frame_token,
      int parent_routing_id,
      int previous_sibling_routing_id);

  // Deletes the RenderFrame in the renderer process.
  // Postcondition: |IsPendingDeletion()| is true.
  void DeleteRenderFrame(mojom::FrameDeleteIntention intent);

  // Track whether the RenderFrame for this RenderFrameHost has been created in
  // or destroyed in the renderer process.
  void RenderFrameCreated();
  void RenderFrameDeleted();

  // Signals that the renderer has requested for this main-frame's window to be
  // shown, at which point we can service navigation requests.
  void Init();

  // This needs to be called to make sure that the parent-child relationship
  // between frames is properly established both for cross-process iframes as
  // well as for inner web contents (i.e. right after having attached it to the
  // outer web contents), so that navigating the whole tree is possible.
  //
  // It's safe to call this method multiple times, or even before the embedding
  // token has been set for this frame, to account for the realistic possibility
  // that inner web contents can became attached to the outer one both before
  // and after the embedding token has been set.
  void PropagateEmbeddingTokenToParentFrame();

  // Returns true if the frame recently plays an audio.
  bool is_audible() const { return is_audible_; }

  // Toggles the audible state of this render frame. This should only be called
  // from AudioStreamMonitor, and should not be invoked with the same value
  // successively.
  void OnAudibleStateChanged(bool is_audible);

  int routing_id() const { return routing_id_; }

  // Called when this frame has added a child. This is a continuation of an IPC
  // that was partially handled on the IO thread (to allocate |new_routing_id|,
  // |frame_token| and |devtools_frame_token|), and is forwarded here. The
  // renderer has already been told to create a RenderFrame with the specified
  // ID values. |browser_interface_broker_receiver| is the receiver end of the
  // BrowserInterfaceBroker interface in the child frame. RenderFrameHost should
  // bind this receiver to expose services to the renderer process. The caller
  // takes care of sending down the client end of the pipe to the child
  // RenderFrame to use.
  void OnCreateChildFrame(
      int new_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const blink::LocalFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::FramePolicy& frame_policy,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType owner_type);

  // Update this frame's state at the appropriate time when a navigation
  // commits. This is called by Navigator::DidNavigate as a helper, in the
  // midst of a DidCommitProvisionalLoad call. If |was_within_same_document| is
  // true the navigation was same-document.
  void DidNavigate(const mojom::DidCommitProvisionalLoadParams& params,
                   NavigationRequest* navigation_request,
                   bool was_within_same_document);

  RenderViewHostImpl* render_view_host() { return render_view_host_.get(); }
  RenderFrameHostDelegate* delegate() { return delegate_; }

  // FrameTree references are being removed from RenderFrameHostImpl as a part
  // of MPArch. Please avoid using these APIs. See crbug.com/1179502 for
  // details.
  FrameTree* frame_tree() const { return frame_tree_; }
  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  // Methods to add/remove/reset/query child FrameTreeNodes of this frame.
  // See class-level comment for FrameTreeNode for how the frame tree is
  // represented.
  size_t child_count() { return children_.size(); }
  FrameTreeNode* child_at(size_t index) const { return children_[index].get(); }
  FrameTreeNode* AddChild(
      std::unique_ptr<FrameTreeNode> child,
      int frame_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      const blink::LocalFrameToken& frame_token,
      const blink::FramePolicy& frame_policy);
  void RemoveChild(FrameTreeNode* child);
  void ResetChildren();

  // Allows FrameTreeNode::SetCurrentURL to update this frame's last committed
  // URL.  Do not call this directly, since we rely on SetCurrentURL to track
  // whether a real load has committed or not.
  void SetLastCommittedUrl(const GURL& url);

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() { return last_successful_url_; }

  // The current URL of the document in the renderer process. Note that this
  // includes URL updates due to document.open() (where it will be updated to
  // the document-open-initiator URL) and special cases like error pages
  // (where kUnreachableWebDataURL is used) and loadDataWithBaseURL() (where the
  // base URL is used), so it might be different than the "last committed
  // URL" provided by GetLastCommittedURL().
  // In almost all cases, use GetLastCommittedURL() instead, as this should only
  // be used when the caller wants to know the current state of the URL in the
  // renderer (e.g. when predicting whether a navigation will do a replacement
  // or not).
  const GURL& last_document_url_in_renderer() const {
    return renderer_url_info_.last_document_url;
  }

  // Whether the last committed document was loaded from loadDataWithBaseURL or
  // not.
  bool was_loaded_from_load_data_with_base_url() const {
    return renderer_url_info_.was_loaded_from_load_data_with_base_url;
  }

  // Saves the URLs and other URL-related information used in the renderer.
  // These values can be used to know the current state of URLs in the renderer.
  // Currently these values are used to simulate calculations in the renderer
  // or to preserve behavior of calculations that used to live in the renderer
  // but was moved to the browser. For most use cases, prefer to use
  // `last_committed_url_` instead.
  struct RendererURLInfo {
    // Tracks this frame's last "document URL", which might be different from:
    // - `last_committed_url_` if the frame did document.open() or sets a
    // different document URL than the committed URL (e.g. loadDataWithBaseURL
    // and error page commits).
    // - The history URL in the renderer (not tracked in the browser) which
    // might be different for error pages, where the document URL will be
    // kUnreachableWebDataURL.
    // Note 1: `last_document_url` might be updated outside of navigation due
    // to document.open(), unlike `last_committed_url_` which can only be
    // updated as a result of navigation. All three URLs are also updated/set to
    // empty when the renderer process crashes.
    // Note 2: This might not have the accurate value of the document URL in the
    // renderer after same-document navigations on error pages or documents
    // loaded through loadDataWithBaseURL(). See comment in GetLastDocumentURL()
    // in render_frame_host_impl.cc for more details.
    GURL last_document_url;

    // Whether the currently committed document is a result of webview's
    // loadDataWithBaseURL API or not.
    bool was_loaded_from_load_data_with_base_url = false;
  };

  // Returns the storage key for the last committed document in this
  // RenderFrameHostImpl. It is used for partitioning storage by the various
  // storage APIs.
  const blink::StorageKey& storage_key() const { return storage_key_; }

  // Returns the http method of the last committed navigation.
  const std::string& last_http_method() { return last_http_method_; }

  // Returns the http status code of the last committed navigation.
  int last_http_status_code() { return last_http_status_code_; }

  // Returns the POST ID of the last committed navigation.
  int64_t last_post_id() { return last_post_id_; }

  // Returns true if last committed navigation's CommonNavigationParam's
  // `has_user_gesture` is true. Should only be used to get the state of the
  // lat navigation, and not the current state of user activation of this
  // RenderFrameHost. See comment on the variable declaration for more details.
  bool last_navigation_started_with_transient_activation() {
    return last_navigation_started_with_transient_activation_;
  }

  // Returns true if `dest_url_info` should be considered the same site as the
  // current contents of this frame. This is the primary entry point for
  // determining if a navigation to `dest_url_info` should stay in this
  // RenderFrameHost's SiteInstance.
  bool IsNavigationSameSite(const UrlInfo& dest_url_info);

  // Returns |frame_origin| if this frame is the top (i.e. root) frame in the
  // frame tree. Otherwise, it returns the top frame's origin.
  const url::Origin& ComputeTopFrameOrigin(
      const url::Origin& frame_origin) const;

  // Computes the IsolationInfo for this frame to `destination`. Set `anonymous`
  // to true if the navigation will be loaded as anonymous document (note that
  // the navigation might be committing an anonymous document even if the
  // document currently loaded in this RFH is not anonymous, and vice versa).
  net::IsolationInfo ComputeIsolationInfoForNavigation(const GURL& destination,
                                                       bool anonymous);

  // Computes the IsolationInfo for this frame to |destination|.
  net::IsolationInfo ComputeIsolationInfoForNavigation(const GURL& destination);

  // Computes the IsolationInfo that should be used for subresources, if
  // |main_world_origin_for_url_loader_factory| is committed to this frame. The
  // boolean `anonymous` specifies whether this frame will commit an anonymous
  // document.
  net::IsolationInfo ComputeIsolationInfoForSubresourcesForPendingCommit(
      const url::Origin& main_world_origin_for_url_loader_factory,
      bool anonymous);

  // Computes site_for_cookies for this frame. A non-empty result denotes which
  // domains are considered first-party to the top-level site when resources are
  // loaded inside this frame. An empty result means that nothing will be
  // first-party, as the frame hierarchy makes this context third-party already.
  //
  // The result can be used to check if cookies (including storage APIs and
  // shared/service workers) are accessible.
  net::SiteForCookies ComputeSiteForCookies();

  // Allows overriding the last committed origin in tests.
  void SetLastCommittedOriginForTesting(const url::Origin& origin);

  // Get HTML data for this RenderFrame by serializing contents on the renderer
  // side and replacing all links to both same-site and cross-site resources
  // with paths to local copies as specified by |url_map| and |frame_token_map|.
  void GetSerializedHtmlWithLocalLinks(
      const base::flat_map<GURL, base::FilePath>& url_map,
      const base::flat_map<blink::FrameToken, base::FilePath>& frame_token_map,
      bool save_with_empty_url,
      mojo::PendingRemote<mojom::FrameHTMLSerializerHandler>
          serializer_handler);

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }
  WebUI::TypeID web_ui_type() const { return web_ui_type_; }

  // Enable Mojo JavaScript bindings in the renderer process. It will be
  // effective on the first creation of script context after the call is made.
  // If called at frame creation time (RenderFrameCreated) or just before a
  // document is committed (ReadyToCommitNavigation), the resulting document
  // will have the JS bindings enabled.
  void EnableMojoJsBindings();

  // Enable Mojo JavaScript bindings in the renderer process, and use the
  // provided BrowserInterfaceBroker to handle JavaScript calls to
  // Mojo.bindInterface. This method should be called in
  // ReadyToCommitNavigation.
  void EnableMojoJsBindingsWithBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker);

  // Returns true if this is a main RenderFrameHost. True if and only if this
  // RenderFrameHost doesn't have a parent.
  bool is_main_frame() const { return !parent_; }

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return is_loading_; }

  // Returns true if this is a top-level frame, or if this frame
  // uses a proxy to communicate with its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() const { return !!GetLocalRenderWidgetHost(); }

  // Returns true if this is not a top-level frame but is still a local root.
  bool is_local_root_subframe() const {
    return !is_main_frame() && is_local_root();
  }

  media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback();

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // Return true if this contains at least one NavigationRequest waiting to
  // commit in this RenderFrameHost. This includes both same-document and
  // cross-document NavigationRequests.
  bool HasPendingCommitNavigation() const;

  // Return true if this contains at least one NavigationRequest waiting to
  // commit in this RenderFrameHost, excluding same-document navigations.
  // NOTE: RenderFrameHostManager surfaces a similar method that will check
  // all the RenderFrameHosts for that FrameTreeNode.
  bool HasPendingCommitForCrossDocumentNavigation() const;

  // Return true if Unload() was called on the frame or one of its ancestors.
  // If true, this corresponds either to unload handlers running for this
  // RenderFrameHost (LifecycleStateImpl::kRunningUnloadHandlers) or when this
  // RenderFrameHost is ready to be deleted
  // (LifecycleStateImpl::kReadyToBeDeleted).
  bool IsPendingDeletion() const;

  // Returns true if this RenderFrameHost is currently stored in the
  // back-forward cache i.e., when lifecycle_state() is kInBackForwardCache.
  bool IsInBackForwardCache() const;

  // Returns a pending same-document navigation request in this frame that has
  // the navigation_token |token|, if any.
  NavigationRequest* GetSameDocumentNavigationRequest(
      const base::UnguessableToken& token);

  // Resets the NavigationRequests stored in this RenderFrameHost.
  void ResetNavigationRequests();

  // Called when a navigation is ready to commit in this
  // RenderFrameHost. Transfers ownership of the NavigationRequest associated
  // with the navigation to this RenderFrameHost.
  void SetNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Tells the renderer that this RenderFrame is being replaced with one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer process keeps the RenderFrameProxy object around as a
  // placeholder while the frame is rendered in a different process.
  //
  // There should always be a |proxy| to replace the old RenderFrameHost. If
  // there are no remaining active views in the process, the proxy will be
  // short-lived and will be deleted when the unload ACK is received.
  //
  // RenderDocument: After a local<->local swap, this function is called with a
  // null |proxy|. It executes common cleanup and marks this RenderFrameHost to
  // have completed its unload handler. The RenderFrameHost may be immediately
  // deleted or deferred depending on its children's unload status.
  void Unload(RenderFrameProxyHost* proxy, bool is_loading);

  // Sent to a renderer when the browser needs to cancel a navigation associated
  // with a speculative RenderFrameHost that has already been asked to commit
  // via `CommitNavigation()`. The renderer will swap out the already-committed
  // RenderFrame, replacing it with a RenderFrameProxy for `proxy`.
  //
  // TODO(https://crbug.com/1220337): This method is fundamentally incompatible
  // with RenderDocument, as there is no RenderFrameProxy to restore for a
  // local<->local swap.
  void UndoCommitNavigation(RenderFrameProxyHost& proxy, bool is_loading);

  // Unload this frame for the proxy. Similar to `Unload()` but without
  // managing the lifecycle of this object.
  void SwapOuterDelegateFrame(RenderFrameProxyHost* proxy);

  // Process the acknowledgment of the unload of this frame from the renderer.
  void OnUnloadACK();

  // Remove this frame and its children. This happens asynchronously, an IPC
  // round trip with the renderer process is needed to ensure children's unload
  // handlers are run.
  // Postcondition: |IsPendingDeletion()| is true.
  void DetachFromProxy();

  // Whether an ongoing navigation in this frame is waiting for a BeforeUnload
  // completion callback either from this RenderFrame or from one of its
  // subframes.
  bool is_waiting_for_beforeunload_completion() const {
    return is_waiting_for_beforeunload_completion_;
  }

  // True if more than |beforeunload_timeout_delay_| has elapsed since starting
  // beforeunload. This may be true before |beforeunload_timeout_| actually
  // fires, as the task can be delayed by task scheduling. See crbug.com/1056257
  bool BeforeUnloadTimedOut() const;

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Called when either the Unload() request has been acknowledged or has timed
  // out.
  void OnUnloaded();

  // Stop the load in progress.
  void Stop();

  // Defines different states the RenderFrameHost can be in during its lifetime
  // i.e., from point of creation to deletion. See |SetLifecycleState|.
  // NOTE: this must be kept consistent with the
  // RenderFrameHostImpl.LifecycleState enum in chrome_track_event.proto for
  // tracing.
  enum class LifecycleStateImpl {
    // This state corresponds to when a speculative RenderFrameHost is created
    // for an ongoing navigation (to new URL) but the navigation hasn't reached
    // ReadyToCommitNavigation stage yet, mainly created for performance
    // optimization. The frame can only be created in this state and no
    // transitions happen to this state.
    //
    // Transitions from this state happen to one of:
    // - kPendingCommit - when cross-RenderFrameHost navigation commits in
    // the renderer and becomes ready to commit and the //content embedders are
    // notified about the navigation's association with this RenderFrameHost.
    // - kActive -- when speculative RenderFrameHost is swapped indirectly
    // instead of following the full navigation path (known as "early commit").
    // This happens when current RenderFrameHost is not live. The work to
    // remove this transition is tracked in crbug.com/1072817.
    //
    // Speculative RenderFrameHost deletion happens without running any unload
    // handlers and with LifecycleStateImpl remaining in kSpeculative state.
    //
    // Note that the term speculative is used, because the navigation might be
    // canceled or redirected and the RenderFrameHost might get deleted before
    // being used.
    kSpeculative,

    // This state corresponds to when a cross-RenderFrameHost navigation is
    // waiting for an acknowledgment from the renderer to swap the
    // RenderFrameHost.
    //
    // Note that cross-document same-RenderFrameHost navigations are not covered
    // by this state, despite going through ReadyToCommitNavigation (the
    // RenderFrameHost will be considered current and be in either kActive or
    // kPrerendering state). The work to eliminate cross-document
    // same-RenderFrameHost navigations is tracked in crbug.com/936696.
    //
    // Transitions from this state happen to one of:
    // - kActive -- when a cross-RenderFrameHost navigation commits inside
    // the primary frame tree.
    // - kPrerendering -- when a cross-RenderFrameHost navigation commits
    // inside prerendered frame tree.
    // - kReadyToBeDeleted -- when the navigation gets aborted. The work to
    // eliminate this is tracked in crbug.com/999255.
    //
    // Transition to this state only happens from kSpeculative state when a
    // speculative RenderFrameHost created for cross-RenderFrameHost navigation
    // commits in the renderer.
    kPendingCommit,

    // Prerender2:
    // This state corresponds to when a RenderFrameHost is the current one in
    // its RenderFrameHostManager and FrameTreeNode for a prerendered frame
    // tree. Documents in this state are invisible to the user and aren't
    // allowed to show any UI changes, but the page is allowed to load and run
    // in the background. Documents in kPrerendering state can be evicted
    // (cancelling prerendering) at any time.
    //
    // A prerendered page is created by an initial navigation in a prerendered
    // frame tree. For the prerendered page to be shown to the user, another
    // navigation in the primary frame tree activates the prerendered page.
    //
    // Transitions from this state happen to one of:
    // - kActive -- when the prerendered page is activated.
    // - kRunningUnloadHandlers -- when a navigation commits in a prerendered
    // frame tree, unloading the previous one.
    // - kReadyToBeDeleted -- when prerendering is cancelled and the prerendered
    // page is deleted.
    //
    // Document can be created in kPrerendering state (while initializing root
    // and child in a prerendered frame tree).
    //
    // Transition to kPrerendering can happen from kPendingCommit (when
    // cross-RenderFrameHost navigation commits inside a prerendered frame
    // tree).
    //
    // Please note that Prerender2 is an experimental feature behind the flag.
    //
    // Note that at the moment, this state is *not* used for RenderFrameHosts in
    // nested FrameTrees inside prerendered pages. See crbug.com/1232528,
    // crbug.com/1244274 for more discussion on whether or not we should support
    // nested FrameTrees inside prerendered pages.
    kPrerendering,

    // This state corresponds to when a RenderFrameHost is the current one in
    // its RenderFrameHostManager/FrameTreeNode inside a primary FrameTree or
    // its descendant FrameTrees. In this state, RenderFrameHost is visible to
    // the user. TODO(crbug.com/1232528, crbug.com/1244274): At the moment,
    // prerendered pages implicitly support nested frame trees, whose
    // RenderFrameHost's are always kActive even though they are not shown to
    // the user. We need to formally determine if prerenders should support
    // nested FrameTrees.
    //
    // Transition to kActive state may happen from one of:
    // - kSpeculative -- when a speculative RenderFrameHost commits to make it
    // the current one in primary frame tree before the corresponding navigation
    // commits.
    // - kPendingCommit -- when a cross-RenderFrameHost navigation commits. The
    // work to eliminate these early commits is tracked in crbug.com/936696
    // - kInBackForwardCache -- when restoring from BackForwardCache.
    // - kPrerendering -- when a prerendered page activates.
    //
    // RenderFrameHost can also be created in this state for an empty document
    // in a FrameTreeNode (e.g initializing root and child in an empty
    // primary FrameTree).
    //
    // Note that this state is also used for nested pages e.g., the
    // RenderFrameHosts in <fencedframe> and <portal> elements, as these nested
    // contexts do not get their own lifecycle state. A RenderFrameHost can tell
    // if it is in a <fencedframe> however, by checking its `FrameTree`'s type.
    // This will be true for portals as well once they are migrated to MPArch.
    kActive,

    // This state corresponds to when RenderFrameHost is stored in
    // BackForwardCache. This happens when the user navigates away from a
    // document, so that the RenderFrameHost can be re-used after a history
    // navigation. Transition to this state happens only from kActive state.
    // BackForwardCache is disabled in prerendering frame trees because a
    // prerendered page is invisible, and the user can't perform any
    // back/forward navigations.
    kInBackForwardCache,

    // This state corresponds to when RenderFrameHost has started running unload
    // handlers (this includes handlers for the "unload", "pagehide", and
    // "visibilitychange" events). An event such as navigation commit or
    // detaching the frame causes the RenderFrameHost to transition to this
    // state. Then, the RenderFrameHost sends IPCs to the renderer process to
    // execute unload handlers and deletes the RenderFrame. The RenderFrameHost
    // waits for an ACK from the renderer process, either
    // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame for a navigating
    // frame or FrameHostMsg_Detach for its subframes, after which the
    // RenderFrameHost transitions to kReadyToBeDeleted state.
    //
    // Transition to this state happens only from kActive and kPrerendering
    // states. Note that eviction from BackForwardCache does not wait for unload
    // handlers, and kInBackForwardCache moves to kReadyToBeDeleted.
    // TODO(https://crbug.com/1222909): Omit unload handling on canceling
    // prerendering, and making kPrerendering move to kReadyToBeDeleted
    // directly.
    kRunningUnloadHandlers,

    // This state corresponds to when RenderFrameHost has completed running the
    // unload handlers. Once all the descendant frames in other processes are
    // gone, this RenderFrameHost will delete itself. Transition to this state
    // may happen from one of kPrerendering, kActive, kInBackForwardCache or
    // kRunningUnloadHandlers states.
    kReadyToBeDeleted,
  };
  LifecycleStateImpl lifecycle_state() const { return lifecycle_state_; }

  // Updates the `lifecycle_state_`. This will also notify the delegate
  // about `RenderFrameHostStateChanged` when the old and new
  // `RenderFrameHost::LifecycleState` changes.
  //
  // When the `new_state == LifecycleStateImpl::kActive`, LifecycleStateImpl of
  // RenderFrameHost and all its children are also updated to
  // `LifecycleStateImpl::kActive`.
  void SetLifecycleState(LifecycleStateImpl new_state);

  // Sets |has_pending_lifecycle_state_update_| to true for this
  // RenderFrameHost and its children. Called when this RenderFrameHost stops
  // being the current one in the RenderFrameHostManager, but its new
  // LifecycleStateImpl is not immediately determined. This boolean is reset
  // when this RenderFrameHost enters the back-forward-cache or the pending
  // deletion list.
  void SetHasPendingLifecycleStateUpdate();

  enum class BeforeUnloadType {
    BROWSER_INITIATED_NAVIGATION,
    RENDERER_INITIATED_NAVIGATION,
    TAB_CLOSE,
    // This reason is used before a tab is discarded in order to free up
    // resources. When this is used and the handler returns a non-empty string,
    // the confirmation dialog will not be displayed and the discard will
    // automatically be canceled.
    DISCARD,
    // This reason is used when preparing a FrameTreeNode for attaching an inner
    // delegate. In this case beforeunload is dispatched in the frame and all
    // the nested child frames.
    INNER_DELEGATE_ATTACH,
  };

  // Runs the beforeunload handler for this frame and its subframes. |type|
  // indicates whether this call is for a navigation or tab close. |is_reload|
  // indicates whether the navigation is a reload of the page.  If |type|
  // corresponds to tab close and not a navigation, |is_reload| should be
  // false.
  void DispatchBeforeUnload(BeforeUnloadType type, bool is_reload);

  // Simulate beforeunload completion callback on behalf of renderer if it's
  // unrenresponsive.
  void SimulateBeforeUnloadCompleted(bool proceed);

  // Returns true if a call to DispatchBeforeUnload will actually send the
  // BeforeUnload IPC.  This can be called on a main frame or subframe.  If
  // |check_subframes_only| is false, it covers handlers for the frame
  // itself and all its descendants.  If |check_subframes_only| is true, it
  // only checks the frame's descendants but not the frame itself.
  bool ShouldDispatchBeforeUnload(bool check_subframes_only);

  // Allow tests to override how long to wait for beforeunload completion
  // callbacks to be invoked before timing out.
  void SetBeforeUnloadTimeoutDelayForTesting(const base::TimeDelta& timeout);

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this frame as focused in the renderer process.  This supports
  // cross-process window.focus() calls.
  void SetFocusedFrame();

  // Continues sequential focus navigation in this frame. |source_proxy|
  // represents the frame that requested a focus change. It must be in the same
  // process as this or |nullptr|.
  void AdvanceFocus(blink::mojom::FocusType type,
                    RenderFrameProxyHost* source_proxy);

  // Get the accessibility mode from the delegate and Send a message to the
  // renderer process to change the accessibility mode.
  void UpdateAccessibilityMode();

#if BUILDFLAG(IS_ANDROID)
  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
  using ExtractSmartClipDataCallback = base::OnceCallback<
      void(const std::u16string&, const std::u16string&, const gfx::Rect&)>;

  void RequestSmartClipExtract(ExtractSmartClipDataCallback callback,
                               gfx::Rect rect);

  void OnSmartClipDataExtracted(int32_t callback_id,
                                const std::u16string& text,
                                const std::u16string& html,
                                const gfx::Rect& clip_rect);
#endif  // BUILDFLAG(IS_ANDROID)

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             mojom::SnapshotAccessibilityTreeParamsPtr params);

  // Resets the accessibility serializer in the renderer.
  void AccessibilityReset();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process.
  void SetAccessibilityCallbackForTesting(
      const AccessibilityCallbackForTesting& callback);

  // Called when the metadata about the accessibility tree for this frame
  // changes due to a browser-side change, as opposed to due to an IPC from
  // a renderer.
  void UpdateAXTreeData();

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // null.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

  // Indicates that this process wants the |untrusted_stack_trace| parameter of
  // FrameHost.DidAddMessageToConsole() to be filled in as much as possible for
  // log_level == kError messages.
  void SetWantErrorMessageStackTrace();

  // Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  // |subresource_loader_params| is used in network service land to pass
  // the parameters to create a custom subresource loader in the renderer
  // process.
  void CommitNavigation(
      NavigationRequest* navigation_request,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      absl::optional<SubresourceLoaderParams> subresource_loader_params,
      absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      const base::UnguessableToken& devtools_navigation_token,
      std::unique_ptr<WebBundleHandle> web_bundle_handle);

  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(
      NavigationRequest* navigation_request,
      const blink::mojom::CommonNavigationParams& common_params,
      const blink::mojom::CommitNavigationParams& commit_params,
      bool has_stale_copy_in_cache,
      int error_code,
      int extended_error_code,
      const absl::optional<std::string>& error_page_content);

  // Sends a renderer-debug URL to the renderer process for handling.
  void HandleRendererDebugURL(const GURL& url);

  // Sets up the Mojo connection between this instance and its associated render
  // frame.
  void SetUpMojoConnection();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void TearDownMojoConnection();

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

  // Creates a WebUI for this RenderFrameHost based on the provided |dest_url|
  // if required. Returns true if a new WebUI was created.
  // If this is a history navigation its NavigationEntry bindings should be
  // provided through |entry_bindings| to allow verifying that they are not
  // being set differently this time around. Otherwise |entry_bindings| should
  // be set to NavigationEntryImpl::kInvalidBindings so that no checks are done.
  bool CreateWebUI(const GURL& dest_url, int entry_bindings);

  // Destroys WebUI instance and resets related data.
  void ClearWebUI();

  // Returns the Mojo ImageDownloader service.
  const mojo::Remote<blink::mojom::ImageDownloader>& GetMojoImageDownloader();

  // Returns remote to renderer side FindInPage associated with this frame.
  const mojo::AssociatedRemote<blink::mojom::FindInPage>& GetFindInPage();

  // Returns associated remote for the blink::mojom::LocalFrame Mojo interface.
  const mojo::AssociatedRemote<blink::mojom::LocalFrame>&
  GetAssociatedLocalFrame();

  // Returns the AgentSchedulingGroupHost associated with this
  // RenderFrameHostImpl.
  virtual AgentSchedulingGroupHost& GetAgentSchedulingGroup();

  // Returns associated remote for the blink::mojom::LocalMainFrame Mojo
  // interface. May be overridden by subclasses, e.g. tests which wish to
  // intercept outgoing local main frame messages.
  virtual blink::mojom::LocalMainFrame* GetAssociatedLocalMainFrame();

  // Returns remote to blink::mojom::HighPriorityLocalFrame Mojo interface. Note
  // this interface is highly experimental and is being tested to address
  // crbug.com/1042118. It is not an associated interface and may be actively
  // reordered. GetAssociatedLocalFrame() should be used in most cases and any
  // additional use cases of this interface should probably consider discussing
  // with navigation-dev@chromium.org first.
  const mojo::Remote<blink::mojom::HighPriorityLocalFrame>&
  GetHighPriorityLocalFrame();

  // Returns associated remote for the blink::mojom::FrameBindingsControl Mojo
  // interface.
  const mojo::AssociatedRemote<mojom::FrameBindingsControl>&
  GetFrameBindingsControl();

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Returns the permissions policy which should be enforced on this
  // RenderFrame.
  const blink::PermissionsPolicy* permissions_policy() const {
    return permissions_policy_.get();
  }

  void ClearFocusedElement();

  bool has_focused_editable_element() const {
    return has_focused_editable_element_;
  }

  // Binds a DevToolsAgent interface for debugging.
  void BindDevToolsAgent(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHost() override;
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif

  // Propagates the visibility state along the immediate local roots by calling
  // RenderWidgetHostViewChildFrame::Show()/Hide(). Calling this on a pending
  // or speculative RenderFrameHost (that has not committed) should be avoided.
  void SetVisibilityForChildViews(bool visible);

  const blink::LocalFrameToken& GetTopFrameToken();

  // Returns an unguessable token for this RFHI.  This provides a temporary way
  // to identify a RenderFrameHost that's compatible with IPC.  Else, one needs
  // to send pid + RoutingID, but one cannot send pid.  One can get it from the
  // channel, but this makes it much harder to get wrong.
  // Once media switches to mojo, we should be able to remove this in favor of
  // sending a mojo overlay factory. Note that this value should only be shared
  // with the renderer process that hosts the frame and other utility processes;
  // it should specifically *not* be shared with any renderer processes that
  // do not host the frame.
  const base::UnguessableToken& GetOverlayRoutingToken() const {
    return frame_token_.value();
  }

  // Binds the receiver end of the BrowserInterfaceBroker interface through
  // which services provided by this RenderFrameHost are exposed to the
  // corresponding RenderFrame. The caller is responsible for plumbing the
  // client end to the renderer process.
  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>);

  // Binds the receiver end of the DomOperationControllerHost interface through
  // which services provided by this RenderFrameHost are exposed to the
  // corresponding RenderFrame. The caller is responsible for plumbing the
  // client end to the renderer process.
  void BindDomOperationControllerHostReceiver(
      mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
          receiver);

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::AssociatedReceiver<mojom::FrameHost>&
  frame_host_receiver_for_testing() {
    return frame_host_associated_receiver_;
  }

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>&
  local_frame_host_receiver_for_testing() {
    return local_frame_host_receiver_;
  }

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::AssociatedReceiver<blink::mojom::LocalMainFrameHost>&
  local_main_frame_host_receiver_for_testing() {
    return local_main_frame_host_receiver_;
  }

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>&
  browser_interface_broker_receiver_for_testing() {
    return broker_receiver_;
  }
  void SetKeepAliveTimeoutForTesting(base::TimeDelta timeout);

  network::mojom::WebSandboxFlags active_sandbox_flags() {
    return policy_container_host_->sandbox_flags();
  }
  bool is_mhtml_document() { return is_mhtml_document_; }

  bool is_overriding_user_agent() { return is_overriding_user_agent_; }

  // Notifies the render frame that |frame_tree_node_| has received user
  // activation. May be invoked multiple times. This is called both for the
  // actual frame that saw user activation and any ancestor frames that might
  // also be activated as part of UserActivationV2 requirements. Does not
  // include frames activated by the same-origin visibility heuristic, see
  // `UserActivationState` for details.
  void DidReceiveUserActivation();

  // Apply any isolation policies, such as site isolation triggered by COOP
  // headers, that might be triggered when a particular frame has just seen a
  // user activation. Called whenever this frame sees a user activation (which
  // may or may not be the first activation in this frame).
  void MaybeIsolateForUserActivation();

  // Returns the current size for this frame.
  const absl::optional<gfx::Size>& frame_size() const { return frame_size_; }

  // Allow tests to override the timeout used to keep subframe processes alive
  // for unload handler processing.
  void SetSubframeUnloadTimeoutForTesting(const base::TimeDelta& timeout);

  // Called when the WebAudio AudioContext given by |audio_context_id| has
  // started (or stopped) playing audible audio.
  void AudioContextPlaybackStarted(int audio_context_id);
  void AudioContextPlaybackStopped(int audio_context_id);

  // Called when this RenderFrameHostImpl enters the BackForwardCache, the
  // document enters in a "Frozen" state where no Javascript can run.
  void DidEnterBackForwardCache();

  // Called when this RenderFrameHostImpl leaves the BackForwardCache. This
  // occurs immediately before a restored document is committed.
  void WillLeaveBackForwardCache();

  // Take ownership over the DidCommitProvisionalLoadParams that were last used
  // to commit this navigation. This is used by the BackForwardCache to
  // re-commit when navigating to a restored page.
  mojom::DidCommitProvisionalLoadParamsPtr TakeLastCommitParams();

  // Start a timer that will evict this RenderFrameHost from the
  // BackForwardCache after time to live.
  void StartBackForwardCacheEvictionTimer();

  bool IsBackForwardCacheDisabled() const;

  // Prevents this frame (along with its parents/children) from being added to
  // the BackForwardCache. If the frame is already in the cache an eviction is
  // triggered.
  void DisableBackForwardCache(BackForwardCache::DisabledSource source,
                               BackForwardCache::DisabledReasonType reason);
  void DisableBackForwardCache(BackForwardCache::DisabledReason reason);
  void ClearDisableBackForwardCache(BackForwardCache::DisabledReason reason);

  bool is_evicted_from_back_forward_cache() {
    return is_evicted_from_back_forward_cache_;
  }

  const std::set<BackForwardCache::DisabledReason>&
  back_forward_cache_disabled_reasons() const {
    return back_forward_cache_disabled_reasons_;
  }

  bool was_restored_from_back_forward_cache_for_debugging() {
    return was_restored_from_back_forward_cache_for_debugging_;
  }

  // Prevents this frame to do a proactive BrowsingInstance swap (for all
  // navigations on this frame - cross-site and same-site).
  void DisableProactiveBrowsingInstanceSwapForTesting();

  bool HasTestDisabledProactiveBrowsingInstanceSwap() const {
    return has_test_disabled_proactive_browsing_instance_swap_;
  }

  void AddServiceWorkerContainerHost(
      const std::string& uuid,
      base::WeakPtr<ServiceWorkerContainerHost> host);
  void RemoveServiceWorkerContainerHost(const std::string& uuid);
  // Returns the last committed ServiceWorkerContainerHost of this frame.
  base::WeakPtr<ServiceWorkerContainerHost> GetLastCommittedServiceWorkerHost();

  // Called to taint |this| so the pages which have requested MediaStream
  // (audio/video/etc capture stream) access would not enter BackForwardCache.
  void OnGrantedMediaStreamAccess();
  bool was_granted_media_access() { return was_granted_media_access_; }

  // Request a new NavigationClient interface from the renderer and returns the
  // ownership of the mojo::AssociatedRemote. This is intended for use by the
  // NavigationRequest.
  mojo::AssociatedRemote<mojom::NavigationClient>
  GetNavigationClientFromInterfaceProvider();

  // Called to signify the RenderFrameHostImpl that one of its ongoing
  // NavigationRequest's has been cancelled.
  void NavigationRequestCancelled(NavigationRequest* navigation_request);

  // Called on the main frame of a page embedded in a Portal when it is
  // activated. The frame has the option to adopt the previous page,
  // |predecessor|, as a portal. The activation can optionally include a message
  // |data| dispatched with the PortalActivateEvent. The |trace_id| is used for
  // generating flow events.
  void OnPortalActivated(
      std::unique_ptr<Portal> predecessor,
      mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalClient>
          client_receiver,
      blink::TransferableMessage data,
      uint64_t trace_id,
      base::OnceCallback<void(blink::mojom::PortalActivateResult)> callback);

  // Called in tests that synthetically create portals but need them to be
  // properly associated with the owning RenderFrameHost.
  void OnPortalCreatedForTesting(std::unique_ptr<Portal> portal);

  // Look up a portal by its token (as received from the renderer process).
  Portal* FindPortalByToken(const blink::PortalToken& portal_token);

  // Return portals owned by |this|.
  std::vector<Portal*> GetPortals() const;

  // Called when a Portal needs to be destroyed.
  void DestroyPortal(Portal* portal);

  // Return fenced frames owned by |this|.
  std::vector<FencedFrame*> GetFencedFrames() const;

  // Called when a fenced frame needs to be destroyed.
  void DestroyFencedFrame(FencedFrame& fenced_frame);

  // Called on the main frame of a page embedded in a Portal to forward a
  // message from the host of a portal.
  void ForwardMessageFromHost(blink::TransferableMessage message,
                              const url::Origin& source_origin);

  void NotifyVirtualKeyboardOverlayRect(const gfx::Rect& keyboard_rect);

  // Returns the keyboard layout mapping.
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap();

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

  // A CommitCallbackInterceptor is used to modify parameters for or cancel a
  // DidCommitNavigation call in tests.
  // WillProcessDidCommitNavigation will be run right after entering a
  // navigation callback and if returning false, will return straight away.
  class CommitCallbackInterceptor {
   public:
    CommitCallbackInterceptor() = default;
    virtual ~CommitCallbackInterceptor() = default;

    virtual bool WillProcessDidCommitNavigation(
        NavigationRequest* navigation_request,
        mojom::DidCommitProvisionalLoadParamsPtr* params,
        mojom::DidCommitProvisionalLoadInterfaceParamsPtr*
            interface_params) = 0;
  };

  // Sets the specified |interceptor|. The caller is responsible for ensuring
  // |interceptor| remains live as long as it is set as the interceptor.
  void SetCommitCallbackInterceptorForTesting(
      CommitCallbackInterceptor* interceptor);

  using CreateNewPopupWidgetCallbackForTesting =
      base::RepeatingCallback<void(RenderWidgetHostImpl*)>;

  // Set a callback to listen to the |CreateNewPopupWidget| for testing.
  void SetCreateNewPopupCallbackForTesting(
      const CreateNewPopupWidgetCallbackForTesting& callback);

  using UnloadACKCallbackForTesting = base::RepeatingCallback<bool()>;

  // Set a callback to listen to the |OnUnloadACK| for testing.
  void SetUnloadACKCallbackForTesting(
      const UnloadACKCallbackForTesting& callback);

  // Posts a message from a frame in another process to the current renderer.
  void PostMessageEvent(
      const absl::optional<blink::RemoteFrameToken>& source_token,
      const std::u16string& source_origin,
      const std::u16string& target_origin,
      blink::TransferableMessage message);

  // Requests to swap the current frame into the frame tree, replacing the
  // RenderFrameProxy it is associated with.
  void SwapIn();

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderFrameHost() const;

  using BackForwardCacheDisablingFeatures =
      blink::scheduler::WebSchedulerTrackedFeatures;
  using BackForwardCacheDisablingFeature =
      blink::scheduler::WebSchedulerTrackedFeature;

  // BackForwardCache disabling feature for |this|, used in determing |this|
  // frame's BackForwardCache eligibility.
  // See comments at |renderer_reported_bfcache_disabling_features_| and
  // |browser_reported_bfcache_disabling_features_|.
  BackForwardCacheDisablingFeatures GetBackForwardCacheDisablingFeatures()
      const;

  using BackForwardCacheDisablingFeaturesCallback =
      base::RepeatingCallback<void(BackForwardCacheDisablingFeatures)>;
  void SetBackForwardCacheDisablingFeaturesCallbackForTesting(
      BackForwardCacheDisablingFeaturesCallback callback) {
    back_forward_cache_disabling_features_callback_for_testing_ = callback;
  }

  // Returns a PrefetchedSignedExchangeCache which is attached to |this|.
  scoped_refptr<PrefetchedSignedExchangeCache>
  EnsurePrefetchedSignedExchangeCache();

  // Clears the entries in the PrefetchedSignedExchangeCache if exists.
  void ClearPrefetchedSignedExchangeCache();

  // Creates a WebBundleHandleTracker from WebBundleHandles which are attached
  // |this| or the parent frame or the opener frame.
  std::unique_ptr<WebBundleHandleTracker> MaybeCreateWebBundleHandleTracker();

  void set_did_stop_loading_callback_for_testing(base::OnceClosure callback) {
    did_stop_loading_callback_ = std::move(callback);
  }

  class BackForwardCacheDisablingFeatureHandle {
   public:
    BackForwardCacheDisablingFeatureHandle();
    BackForwardCacheDisablingFeatureHandle(
        BackForwardCacheDisablingFeatureHandle&&);
    BackForwardCacheDisablingFeatureHandle& operator=(
        BackForwardCacheDisablingFeatureHandle&& other) {
      feature_ = other.feature_;
      render_frame_host_ = other.render_frame_host_;
      other.render_frame_host_ = nullptr;
      return *this;
    }

    inline ~BackForwardCacheDisablingFeatureHandle() { reset(); }

    // This will reduce the feature count for |feature_| for the first time, and
    // do nothing for further calls.
    inline void reset() {
      if (render_frame_host_)
        render_frame_host_->OnBackForwardCacheDisablingFeatureRemoved(feature_);
      render_frame_host_ = nullptr;
    }

   private:
    friend class RenderFrameHostImpl;
    BackForwardCacheDisablingFeatureHandle(
        RenderFrameHostImpl* render_frame_host,
        BackForwardCacheDisablingFeature feature);

    base::WeakPtr<RenderFrameHostImpl> render_frame_host_ = nullptr;
    BackForwardCacheDisablingFeature feature_;
  };

  // A feature that blocks back/forward cache is used. This function is used for
  // non sticky blocking features.
  BackForwardCacheDisablingFeatureHandle
  RegisterBackForwardCacheDisablingNonStickyFeature(
      BackForwardCacheDisablingFeature feature);

  // A feature that blocks back/forward cache is used. This function is used for
  // sticky blocking features.
  void OnBackForwardCacheDisablingStickyFeatureUsed(
      BackForwardCacheDisablingFeature feature);

  // Returns true if the frame is frozen.
  bool IsFrozen();

  // Set the `frame_` for sending messages to the renderer process.
  void SetMojomFrameRemote(mojo::PendingAssociatedRemote<mojom::Frame>);

  void GetAudioContextManager(
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);

  void GetFileSystemManager(
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver);

  void GetGeolocationService(
      mojo::PendingReceiver<blink::mojom::GeolocationService> receiver);

  void GetDeviceInfoService(
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  void GetManagedConfigurationService(
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);

  void GetFontAccessManager(
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  void BindComputePressureHost(
      mojo::PendingReceiver<blink::mojom::ComputePressureHost> receiver);

  void GetFileSystemAccessManager(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver);

#if !BUILDFLAG(IS_ANDROID)
  void GetHidService(mojo::PendingReceiver<blink::mojom::HidService> receiver);

  void BindSerialService(
      mojo::PendingReceiver<blink::mojom::SerialService> receiver);
#endif

  IdleManagerImpl* GetIdleManager();

  void BindIdleManager(
      mojo::PendingReceiver<blink::mojom::IdleManager> receiver);

  void GetPresentationService(
      mojo::PendingReceiver<blink::mojom::PresentationService> receiver);

  PresentationServiceImpl& GetPresentationServiceForTesting();

  void GetSpeechSynthesis(
      mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver);

  void CreateLockManager(
      mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  void CreateIDBFactory(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);

  void CreateBucketManagerHost(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

  void GetSensorProvider(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver);

  void CreatePermissionService(
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver);

  void CreatePaymentManager(
      mojo::PendingReceiver<payments::mojom::PaymentManager> receiver);

  void CreateWebBluetoothService(
      mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver);

  void GetWebAuthenticationService(
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);

  void GetVirtualAuthenticatorManager(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver);

  void GetPushMessaging(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);

  void CreateDedicatedWorkerHostFactory(
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);

  void CreateWebTransportConnector(
      mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver);

  void CreateNotificationService(
      mojo::PendingReceiver<blink::mojom::NotificationService> receiver);

  void CreateInstalledAppProvider(
      mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver);

  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);
  void CreateCodeCacheHostWithIsolationKey(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
      const net::NetworkIsolationKey& nik);

#if BUILDFLAG(IS_ANDROID)
  void BindNFCReceiver(mojo::PendingReceiver<device::mojom::NFC> receiver);
#endif

  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

  void BindInputInjectorReceiver(
      mojo::PendingReceiver<mojom::InputInjector> receiver);

  void BindWebOTPServiceReceiver(
      mojo::PendingReceiver<blink::mojom::WebOTPService> receiver);

  void BindFederatedAuthRequestReceiver(
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver);

  void BindFederatedAuthResponseReceiver(
      mojo::PendingReceiver<blink::mojom::FederatedAuthResponse> receiver);

  void BindRestrictedCookieManager(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver);
  void BindRestrictedCookieManagerWithOrigin(
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      const net::IsolationInfo& isolation_info,
      const url::Origin& origin);

  // Requires the following preconditions, reporting a bad message otherwise.
  //
  // 1. This frame's top-frame origin must be potentially trustworthy and
  // have scheme HTTP or HTTPS. (See network::SuitableTrustTokenOrigin's class
  // comment for the rationale.)
  //
  // 2. Trust Tokens must be enabled (network::features::kTrustTokens).
  //
  // 3. This frame's origin must be potentially trustworthy.
  void BindHasTrustTokensAnswerer(
      mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver);

  // Creates connections to WebUSB interfaces bound to this frame.
  void CreateWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver);

  void BindMediaInterfaceFactoryReceiver(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  void BindMediaMetricsProviderReceiver(
      mojo::PendingReceiver<media::mojom::MediaMetricsProvider> receiver);

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  void BindMediaRemoterFactoryReceiver(
      mojo::PendingReceiver<media::mojom::RemoterFactory> receiver);
#endif

  void CreateAudioInputStreamFactory(
      mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
          receiver);

  void CreateAudioOutputStreamFactory(
      mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
          receiver);

  void GetFeatureObserver(
      mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver);

  void BindRenderAccessibilityHost(
      mojo::PendingReceiver<mojom::RenderAccessibilityHost> receiver);

  // Prerender2:
  // Tells PrerenderHostRegistry to cancel the prerendering of the page this
  // frame is in, which destroys this frame.
  // Returns true if a prerender was canceled. Does nothing and returns false if
  // `this` is not prerendered.
  bool CancelPrerendering(PrerenderHost::FinalStatus status);
  // Called by MojoBinderPolicyApplier when it receives a kCancel interface.
  void CancelPrerenderingByMojoBinderPolicy(const std::string& interface_name);

  // Prerender2:
  // Called when the Activate IPC is sent to the renderer. Puts the
  // MojoPolicyBinderApplier in "loose" mode via PrepareToGrantAll() until
  // DidActivateForPrerending() is called.
  void RendererWillActivateForPrerendering();

  // Prerender2:
  // Called when the Activate IPC is acknowledged by the renderer. Relinquishes
  // the MojoPolicyBinderApplier.
  void RendererDidActivateForPrerendering();

  // https://mikewest.github.io/corpp/#initialize-embedder-policy-for-global
  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    return policy_container_host_->cross_origin_embedder_policy();
  }
  CrossOriginEmbedderPolicyReporter* coep_reporter() {
    return coep_reporter_.get();
  }
  void SetCrossOriginOpenerPolicyReporter(
      std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter);

  // Semi-formal definition of COOP:
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
  network::CrossOriginOpenerPolicy cross_origin_opener_policy() const {
    return policy_container_host_->cross_origin_opener_policy();
  }

  CrossOriginOpenerPolicyAccessReportManager* coop_access_report_manager() {
    return &coop_access_report_manager_;
  }
  int virtual_browsing_context_group() const {
    return virtual_browsing_context_group_;
  }
  int soap_by_default_virtual_browsing_context_group() const {
    return soap_by_default_virtual_browsing_context_group_;
  }

  const network::mojom::ContentSecurityPolicy* required_csp() {
    return required_csp_.get();
  }

  bool anonymous() const { return anonymous_; }

  PolicyContainerHost* policy_container_host() {
    return policy_container_host_.get();
  }

  // This is used by RenderFrameHostManager to ensure the replacement
  // RenderFrameHost is properly initialized when performing an early commit
  // as a recovery for a crashed frame.
  // TODO(https://crbug.com/1072817): Remove this logic when removing the
  // early commit.
  void SetPolicyContainerForEarlyCommitAfterCrash(
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // A counter which is incremented by one every time `CommitNavigation` or
  // `CommitFailedNavigation` is sent to the renderer.
  int commit_navigation_sent_counter() {
    return commit_navigation_sent_counter_;
  }

  // This function mimics DidCommitProvisionalLoad for page activation
  // (back-forward cache restore or prerender activation).
  void DidCommitPageActivation(NavigationRequest* committing_navigation_request,
                               mojom::DidCommitProvisionalLoadParamsPtr params);

  // Whether there's any "unload" event handlers registered on this
  // RenderFrameHost or subframes that share the same SiteInstance as this
  // RenderFrameHost.
  bool UnloadHandlerExistsInSameSiteInstanceSubtree();

  bool has_unload_handler() const { return has_unload_handler_; }

  bool has_committed_any_navigation() const {
    return has_committed_any_navigation_;
  }

  // Return true if the process this RenderFrameHost is using has crashed and we
  // are replacing RenderFrameHosts for crashed frames rather than reusing them.
  //
  // This is not exactly the opposite of IsRenderFrameLive().
  // IsRenderFrameLive() is false when the RenderProcess died, but it is also
  // false when it hasn't been initialized.
  bool must_be_replaced() const { return must_be_replaced_; }
  // Resets the must_be_replaced after the RFH has been reinitialized. Do not
  // add any more usages of this.
  // TODO(https://crbug.com/936696): Remove this.
  void reset_must_be_replaced() { must_be_replaced_ = false; }

  int renderer_exit_count() const { return renderer_exit_count_; }

  // Re-creates loader factories and pushes them to |RenderFrame|.
  // Used in case we need to add or remove intercepting proxies to the
  // running renderer, or in case of Network Service connection errors.
  void UpdateSubresourceLoaderFactories();

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateCrossOriginPrefetchLoaderFactoryBundle();

  // Returns the BackForwardCacheMetrics associated with the last
  // NavigationEntry this RenderFrameHostImpl committed.
  BackForwardCacheMetrics* GetBackForwardCacheMetrics();

  blink::mojom::PreferredColorScheme GetPreferredColorScheme() const {
    return preferred_color_scheme_;
  }

  // Returns a base salt used to generate frame-specific IDs for media-device
  // enumerations.
  const std::string& GetMediaDeviceIDSaltBase() const {
    return media_device_id_salt_base_;
  }

  // Returns the topmost ancestor RenderFrameHost. This includes any parents (in
  // the case of subframes), any outer documents (e.g. fenced frame owners), and
  // any GuestViews. See also GetOutermostMainFrame which does not escape
  // GuestViews and GetParentOrOuterDocumentOrEmbedder for more details.
  // Note that this may be different from getting the WebContents' primary main
  // frame. For example, if `this` is in a bfcached or prerendered page, this
  // will return the cached/prerendered page's main RenderFrameHost.
  RenderFrameHostImpl* GetOutermostMainFrameOrEmbedder();

  void set_inner_tree_main_frame_tree_node_id(int id) {
    inner_tree_main_frame_tree_node_id_ = id;
  }
  int inner_tree_main_frame_tree_node_id() const {
    return inner_tree_main_frame_tree_node_id_;
  }

  // These are the content internal equivalents of
  // |RenderFrameHost::ForEachRenderFrameHost| whose comment can be referred to
  // for details. Content internals can also access speculative
  // RenderFrameHostImpls if necessary by using the
  // |ForEachRenderFrameHostIncludingSpeculative| variations.
  using FrameIterationCallbackImpl =
      base::RepeatingCallback<FrameIterationAction(RenderFrameHostImpl*)>;
  using FrameIterationAlwaysContinueCallbackImpl =
      base::RepeatingCallback<void(RenderFrameHostImpl*)>;
  void ForEachRenderFrameHost(FrameIterationCallbackImpl on_frame);
  void ForEachRenderFrameHost(
      FrameIterationAlwaysContinueCallbackImpl on_frame);
  void ForEachRenderFrameHostIncludingSpeculative(
      FrameIterationCallbackImpl on_frame);
  void ForEachRenderFrameHostIncludingSpeculative(
      FrameIterationAlwaysContinueCallbackImpl on_frame);

  // |ForEachRenderFrameHost| has multiple overloads for convenience of the
  // caller that only differ by the provided callback's signature.
  // |FrameIterationWrapper| converts to a common callback signature for the
  // implementation to use.
  template <typename RfhType>
  static FrameIterationCallbackImpl FrameIterationWrapper(
      base::RepeatingCallback<FrameIterationAction(RfhType*)> on_frame) {
    return base::BindRepeating(
        [](base::RepeatingCallback<FrameIterationAction(RfhType*)> on_frame,
           RenderFrameHostImpl* rfh) { return on_frame.Run(rfh); },
        on_frame);
  }
  template <typename RfhType>
  static FrameIterationCallbackImpl FrameIterationWrapper(
      base::RepeatingCallback<void(RfhType*)> on_frame) {
    return base::BindRepeating(
        [](base::RepeatingCallback<void(RfhType*)> on_frame,
           RenderFrameHostImpl* rfh) {
          on_frame.Run(rfh);
          return FrameIterationAction::kContinue;
        },
        on_frame);
  }

  bool DocumentUsedWebOTP() override;

  scoped_refptr<WebAuthRequestSecurityChecker>
  GetWebAuthRequestSecurityChecker();

  base::WeakPtr<RenderFrameHostImpl> GetWeakPtr();

  // blink::mojom::LocalFrameHost
  void EnterFullscreen(blink::mojom::FullscreenOptionsPtr options,
                       EnterFullscreenCallback callback) override;
  void ExitFullscreen() override;
  void FullscreenStateChanged(
      bool is_fullscreen,
      blink::mojom::FullscreenOptionsPtr options) override;
  void RegisterProtocolHandler(const std::string& scheme,
                               const GURL& url,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(const std::string& scheme,
                                 const GURL& url,
                                 bool user_gesture) override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void MainDocumentElementAvailable(bool uses_temporary_zoom_level) override;
  void SetNeedsOcclusionTracking(bool needs_tracking) override;
  void SetVirtualKeyboardOverlayPolicy(bool vk_overlays_content) override;
  void VisibilityChanged(blink::mojom::FrameVisibility) override;
  void DidChangeThemeColor(absl::optional<SkColor> theme_color) override;
  void DidChangeBackgroundColor(SkColor background_color,
                                bool color_adjust) override;
  void DidFailLoadWithError(const GURL& url, int32_t error_code) override;
  void DidFocusFrame() override;
  void DidCallFocus() override;
  void EnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy) override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void SuddenTerminationDisablerChanged(
      bool present,
      blink::mojom::SuddenTerminationDisablerType disabler_type) override;
  void HadStickyUserActivationBeforeNavigationChanged(bool value) override;
  void ScrollRectToVisibleInParentFrame(
      const gfx::Rect& rect_to_scroll,
      blink::mojom::ScrollIntoViewParamsPtr params) override;
  void BubbleLogicalScrollInParentFrame(
      blink::mojom::ScrollDirection direction,
      ui::ScrollGranularity granularity) override;
  void DidBlockNavigation(
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  void DidChangeLoadProgress(double load_progress) override;
  void DidFinishLoad(const GURL& validated_url) override;
  void DispatchLoad() override;
  void GoToEntryAtOffset(int32_t offset, bool has_user_gesture) override;
  void NavigateToAppHistoryKey(const std::string& key,
                               bool has_user_gesture) override;
  void UpdateTitle(const absl::optional<::std::u16string>& title,
                   base::i18n::TextDirection title_direction) override;
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override;
  void HandleAccessibilityFindInPageResult(
      blink::mojom::FindInPageResultAXParamsPtr params) override;
  void HandleAccessibilityFindInPageTermination() override;
  void DocumentOnLoadCompleted() override;
  void ForwardResourceTimingToParent(
      blink::mojom::ResourceTimingInfoPtr timing) override;
  void DidDispatchDOMContentLoadedEvent() override;
  void RunModalAlertDialog(const std::u16string& alert_message,
                           bool disable_third_party_subframe_suppresion,
                           RunModalAlertDialogCallback callback) override;
  void RunModalConfirmDialog(const std::u16string& alert_message,
                             bool disable_third_party_subframe_suppresion,
                             RunModalConfirmDialogCallback callback) override;
  void RunModalPromptDialog(const std::u16string& alert_message,
                            const std::u16string& default_value,
                            bool disable_third_party_subframe_suppresion,
                            RunModalPromptDialogCallback callback) override;
  void RunBeforeUnloadConfirm(bool is_reload,
                              RunBeforeUnloadConfirmCallback callback) override;
  void UpdateFaviconURL(
      std::vector<blink::mojom::FaviconURLPtr> favicon_urls) override;
  void DownloadURL(blink::mojom::DownloadURLParamsPtr params) override;
  void FocusedElementChanged(bool is_editable_element,
                             const gfx::Rect& bounds_in_frame_widget,
                             blink::mojom::FocusType focus_type) override;
  void TextSelectionChanged(const std::u16string& text,
                            uint32_t offset,
                            const gfx::Range& range) override;
  void ShowPopupMenu(
      mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      std::vector<blink::mojom::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
  void CreateNewPopupWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
          blink_popup_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget)
      override;
  void ShowContextMenu(
      mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
          context_menu_client,
      const blink::UntrustworthyContextMenuParams& params) override;
  void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& http_method,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination,
      bool include_credentials) override;
  void DidChangeFrameOwnerProperties(
      const blink::FrameToken& child_frame_token,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties) override;
  void DidChangeOpener(
      const absl::optional<blink::LocalFrameToken>& opener_frame) override;
  void DidChangeIframeAttributes(
      const blink::FrameToken& child_frame_token,
      network::mojom::ContentSecurityPolicyPtr parsed_csp_attribute,
      bool anonymous) override;
  void DidChangeFramePolicy(const blink::FrameToken& child_frame_token,
                            const blink::FramePolicy& frame_policy) override;
  void CapturePaintPreviewOfSubframe(
      const gfx::Rect& clip_rect,
      const base::UnguessableToken& guid) override;
  void SetCloseListener(
      mojo::PendingRemote<blink::mojom::CloseListener> listener) override;
  void Detach() override;
  void DidAddMessageToConsole(
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      uint32_t line_no,
      const absl::optional<std::u16string>& source_id,
      const absl::optional<std::u16string>& untrusted_stack_trace) override;
  void FrameSizeChanged(const gfx::Size& frame_size) override;
  void DidChangeSrcDoc(const blink::FrameToken& child_frame_token,
                       const std::string& srcdoc_value) override;

  // blink::mojom::BackForwardCacheControllerHost:
  void EvictFromBackForwardCache(blink::mojom::RendererEvictionReason) override;
  void DidChangeBackForwardCacheDisablingFeatures(
      uint64_t features_mask) override;

  // blink::LocalMainFrameHost overrides:
  void ScaleFactorChanged(float scale) override;
  void ContentsPreferredSizeChanged(const gfx::Size& pref_size) override;
  void TextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr page_info) override;
  void FocusPage() override;
  void TakeFocus(bool reverse) override;
  void UpdateTargetURL(const GURL& url,
                       blink::mojom::LocalMainFrameHost::UpdateTargetURLCallback
                           callback) override;
  void RequestClose() override;
  void ShowCreatedWindow(const blink::LocalFrameToken& opener_frame_token,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture,
                         ShowCreatedWindowCallback callback) override;
  void SetWindowRect(const gfx::Rect& bounds,
                     SetWindowRectCallback callback) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidAccessInitialMainDocument() override;

  void ReportNoBinderForInterface(const std::string& error);

  // Returns true if this object has any NavigationRequests matching |origin|.
  // Since this function is used to find existing committed/committing origins
  // that have not opted-in to isolation, and since any calls to this function
  // will be initiated by a NavigationRequest that is itself requesting opt-in
  // isolation, |navigation_request_to_exclude| allows that request to exclude
  // itself from consideration.
  bool HasCommittingNavigationRequestForOrigin(
      const url::Origin& origin,
      NavigationRequest* navigation_request_to_exclude);

  // Force the RenderFrameHost to be left in pending deletion state instead of
  // being actually deleted after navigating away:
  // - Force waiting for unload handler result regardless of whether an
  //   unload handler is present or not.
  // - Disable unload timeout monitor.
  // - Ignore any OnUnloadACK sent by the renderer process.
  void DoNotDeleteForTesting();

  // This method will unset the flag |do_not_delete_for_testing_| to resume
  // deletion on the RenderFrameHost. Deletion will only be triggered if
  // RenderFrameHostImpl::Detach() is called for the RenderFrameHost. This is a
  // counterpart for DoNotDeleteForTesting() which sets the flag
  // |do_not_delete_for_testing_|.
  void ResumeDeletionForTesting();

  // This method will detach forcely RenderFrameHost with setting the states,
  // |do_not_delete_for_testing_| and |detach_state_|, to resume deletion on
  // the RenderFrameHost.
  void DetachForTesting();

  // Document-associated data. This is cleared whenever a new document is hosted
  // by this RenderFrameHost. Please refer to the description at
  // content/public/browser/document_user_data.h for more details.
  base::SupportsUserData::Data* GetDocumentUserData(const void* key) const {
    return document_associated_data_->GetUserData(key);
  }

  void SetDocumentUserData(const void* key,
                           std::unique_ptr<base::SupportsUserData::Data> data) {
    document_associated_data_->SetUserData(key, std::move(data));
  }

  void RemoveDocumentUserData(const void* key) {
    document_associated_data_->RemoveUserData(key);
  }

  void AddDocumentService(internal::DocumentServiceBase* document_service,
                          base::PassKey<internal::DocumentServiceBase>);
  void RemoveDocumentService(internal::DocumentServiceBase* document_service,
                             base::PassKey<internal::DocumentServiceBase>);

  // Called when we commit speculative RFH early due to not having an alive
  // current frame. This happens when the renderer crashes before navigating to
  // a new URL using speculative RenderFrameHost.
  // TODO(https://crbug.com/1072817): Undo this plumbing after removing the
  // early post-crash CommitPending() call.
  void OnCommittedSpeculativeBeforeNavigationCommit() {
    committed_speculative_rfh_before_navigation_commit_ = true;
  }

  // Returns the child frame if |child_frame_routing_id| is an immediate child
  // of this RenderFrameHost. |child_frame_routing_id| is considered untrusted,
  // so the renderer process is killed if it refers to a RenderFrameHostImpl
  // that is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(int32_t child_frame_routing_id,
                                    bad_message::BadMessageReason reason);

  // Returns the child frame if |child_frame_token| is an immediate child of
  // this RenderFrameHostImpl. |child_frame_token| is considered untrusted, so
  // the renderer process is killed if it refers to a RenderFrameHostImpl that
  // is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(const blink::FrameToken& child_frame_token,
                                    bad_message::BadMessageReason reason);

  // Whether we should run the pagehide/visibilitychange handlers of the
  // RenderFrameHost we're navigating away from (|old_frame_host|) during the
  // commit to a new RenderFrameHost (this RenderFrameHost). Should only return
  // true when we're doing a same-site navigation and we did a proactive
  // BrowsingInstance swap but we're reusing the old page's renderer process.
  // We should run pagehide and visibilitychange handlers of the old page during
  // the commit of the new main frame in those cases because in other same-site
  // navigations we will run those handlers before the new page finished
  // committing. Note that unload handlers will still run after the new page
  // finished committing. Ideally we would run unload handlers alongside
  // pagehide and visibilitychange handlers at commit time too, but we'd need to
  // actually unload/freeze the page in that case which is more complex.
  // TODO(crbug.com/1110744): Support unload-in-commit.
  bool ShouldDispatchPagehideAndVisibilitychangeDuringCommit(
      RenderFrameHostImpl* old_frame_host,
      const UrlInfo& dest_url_info);

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateURLLoaderNetworkObserver();

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserver();

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(
      network::mojom::CookieAccessDetailsPtr details) override;

  void GetSavableResourceLinksFromRenderer();

  // Helper for checking if a navigation to an error page should be excluded
  // from CanAccessDataForOrigin and/or CanCommitOriginAndUrl security checks.
  //
  // It is allowed for |navigation_request| to be null - for example when
  // committing a same-document navigation.
  //
  // The optional |should_commit_unreachable_url| will be set to |true| if the
  // caller should verify that DidCommitProvisionalLoadParams'
  // url_is_unreachable is |true|.
  bool ShouldBypassSecurityChecksForErrorPage(
      NavigationRequest* navigation_request,
      bool* should_commit_error_page = nullptr);

  // Explicitly allow the use of an audio output device in this render frame.
  // When called with a hashed device id string the renderer will be allowed to
  // use the associated device for audio output until this method is called
  // again with a different hashed device id or the origin changes. To remove
  // this permission, this method may be called with the empty string.
  void SetAudioOutputDeviceIdForGlobalMediaControls(
      std::string hashed_device_id);

  // Evicts the document from the BackForwardCache if it is in the cache,
  // and ineligible for caching.
  void MaybeEvictFromBackForwardCache();

  // Returns a filter that should be associated with all AssociatedReceivers for
  // this frame. |interface_name| is used for logging purposes and must be valid
  // for the entire program duration.
  std::unique_ptr<mojo::MessageFilter> CreateMessageFilterForAssociatedReceiver(
      const char* interface_name);

  int accessibility_fatal_error_count_for_testing() const {
    return accessibility_fatal_error_count_;
  }

  // TODO(https://crbug.com/1179502): FrameTree and FrameTreeNode are not const
  // as with prerenderer activation the page needs to move between
  // FrameTreeNodes and FrameTrees. Note that FrameTreeNode can only change for
  // root nodes. As it's hard to make sure that all places handle this
  // transition correctly, MPArch will remove references from this class to
  // FrameTree/FrameTreeNode.
  void SetFrameTreeNode(FrameTreeNode& frame_tree_node);
  void SetFrameTree(FrameTree& frame_tree);

  // Builds and return a ClientSecurityState based on the internal
  // RenderFrameHostImpl state. This is never null.
  network::mojom::ClientSecurityStatePtr BuildClientSecurityState() const;

  void OnDidRunInsecureContent(const GURL& security_origin,
                               const GURL& target_url);
  void OnDidDisplayContentWithCertificateErrors();
  void OnDidRunContentWithCertificateErrors();

  PeerConnectionTrackerHost& GetPeerConnectionTrackerHost();
  void BindPeerConnectionTrackerHost(
      mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost> receiver);
  void EnableWebRtcEventLogOutput(int lid, int output_period_ms) override;
  void DisableWebRtcEventLogOutput(int lid) override;
  bool IsDocumentOnLoadCompletedInPrimaryMainFrame() override;
  const std::vector<blink::mojom::FaviconURLPtr>& FaviconURLs() override;

#if BUILDFLAG(ENABLE_MDNS)
  void CreateMdnsResponder(
      mojo::PendingReceiver<network::mojom::MdnsResponder> receiver);
#endif  // BUILDFLAG(ENABLE_MDNS)

#if BUILDFLAG(ENABLE_PLUGINS)
  void PepperInstanceClosed(int32_t instance_id);
  void PepperSetVolume(int32_t instance_id, double volume);
#endif

  const network::mojom::URLResponseHeadPtr& last_response_head() const {
    return last_response_head_;
  }

  NavigationEarlyHintsManager* early_hints_manager() {
    return early_hints_manager_.get();
  }

  // Create a bundle of subresource factories for an initial empty document.
  // Used for browser-initiated (e.g. no opener) creation of a new main frame.
  // Example scenarios:
  // 1. `window.open` with `rel=noopener` (this path is not used if rel=noopener
  //    is missing;  child frames also don't go through this path).
  // 2. Browser-initiated (e.g. browser-UI-driven) opening of a new tab.
  // 3. Recreating an active main frame when recovering from a renderer crash.
  // 4. Creating a speculative main frame if navigation requires a process swap.
  //
  // Currently the returned bundle is mostly empty - in practice it is
  // sufficient to provide only a NetworkService-bound default factory (i.e. no
  // chrome-extension:// or file:// or data: factories are present today).
  // TODO(lukasza): Revisit the above is necessary.
  //
  // The parameters of the NetworkService-bound default factory (e.g.
  // `request_initiator_origin_lock`, IPAddressSpace, etc.) are associated
  // with the current `last_committed_origin_` (typically an opaque, unique
  // origin for a browser-created initial empty document;  it may be a regular
  // origin when recovering from a renderer crash).
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateSubresourceLoaderFactoriesForInitialEmptyDocument();

  // Prerender2:
  // Dispatches DidFinishLoad and DOMContentLoaded if it occurred pre-activation
  // and was deferred to be dispatched after activation.
  void MaybeDispatchDOMContentLoadedOnPrerenderActivation();
  void MaybeDispatchDidFinishLoadOnPrerenderActivation();

  // Returns the document owning the frame this RenderFrameHost is located
  // in, which will either be a parent (for <iframe>s) or outer document (for
  // <fencedframe>, <portal> or an embedder (e.g. GuestViews)). See
  // `RenderFrameHost::GetParentOrOuterDocument` for the version of this API
  // that does not cross a browsing session boundary (ie. Not escaping a
  // GuestView). This method typically will be used for input, compositing, and
  // focus related functionality where the physical arrangement of frames, as
  // opposed to their semantics is required. Example:
  //  A (GuestView embedder)
  //   B (<webview> - placeholder frame)
  //    B* (embedded document main frame)
  //     C (iframe)
  //
  //  C GetParent & GetParentOrOuterDocumentOrEmbedder returns B*.
  //  B* GetParent & GetParentOrOuterDocument returns null.
  //  B* GetParentOrOuterDocumentOrEmbedder returns A.
  //  B GetParent & GetParentOrOuterDocumentOrEmbedder returns A.
  //  A GetParent & GetParentOrOuterDocumentOrEmbedder returns
  //  nullptr.
  RenderFrameHostImpl* GetParentOrOuterDocumentOrEmbedder();

  static const char* LifecycleStateImplToString(LifecycleStateImpl state);

  // Computes the nonce to be used for isolation info and storage key.
  absl::optional<base::UnguessableToken> ComputeNonce(bool anonymous);

  // Return the frame immediately preceding this RenderFrameHost in its parent's
  // children, or nullptr if there is no such node.
  FrameTreeNode* PreviousSibling() const;

  // Return the frame immediately following this RenderFrameHost in its parent's
  // children, or nullptr if there is no such node.
  FrameTreeNode* NextSibling() const;

  // Set the |last_committed_origin_|, |isolation_info_|, and
  // |permissions_policy_| of |this| frame, inheriting the origin from
  // |new_frame_creator| as appropriate (e.g. depending on whether |this| frame
  // should be sandboxed / should have an opaque origin instead).
  void SetOriginDependentStateOfNewFrame(const url::Origin& new_frame_creator);

  // Returns the "top_level_site" that should be used to calculate storage key.
  // It correspond to the top-level document's origin, unless its is an
  // extension with host permissions to its direct child. In this case, this
  // returns the child's origin.
  url::Origin CalculateTopLevelOriginForStorageKey(
      const url::Origin& new_rfh_origin);

  // Returns the BrowsingContextState associated with this RenderFrameHostImpl.
  // See class comments in BrowsingContextState for a more detailed description.
  scoped_refptr<BrowsingContextState>& browsing_context_state() {
    return browsing_context_state_;
  }

  // Retrieve proxies in a way that is no longer dependent on access to
  // FrameTreeNode or RenderFrameHostManager.
  RenderFrameProxyHost* GetProxyToParent();
  RenderFrameProxyHost* GetProxyToOuterDelegate();

  void DidChangeReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy);

  class CheckOnDeleteRef {
   public:
    CheckOnDeleteRef(const CheckOnDeleteRef&) = delete;
    CheckOnDeleteRef& operator=(const CheckOnDeleteRef&) = delete;
    ~CheckOnDeleteRef();

   private:
    friend class RenderFrameHostImpl;

    explicit CheckOnDeleteRef(RenderFrameHostImpl* host);

    RenderFrameHostImpl* host_;
  };

  // TODO(https://crbug.com/1262098): used to track down crash.
  std::unique_ptr<CheckOnDeleteRef> EnableCheckIfDeleted();

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  // |lifecycle_state_| can either be kSpeculative, kPrerendering, or kActive
  // during RenderFrameHostImpl creation.
  RenderFrameHostImpl(
      SiteInstance* site_instance,
      scoped_refptr<RenderViewHostImpl> render_view_host,
      RenderFrameHostDelegate* delegate,
      FrameTree* frame_tree,
      FrameTreeNode* frame_tree_node,
      int32_t routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      const blink::LocalFrameToken& frame_token,
      bool renderer_initiated_creation_of_main_frame,
      LifecycleStateImpl lifecycle_state,
      scoped_refptr<BrowsingContextState> browsing_context_state);

  // The SendCommit* functions below are wrappers for commit calls
  // made to mojom::NavigationClient.
  // These exist to be overridden in tests to retain mojo callbacks.
  virtual void SendCommitNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      blink::mojom::PolicyContainerPtr policy_container,
      const base::UnguessableToken& devtools_navigation_token);
  virtual void SendCommitFailedNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool has_stale_copy_in_cache,
      int32_t error_code,
      int32_t extended_error_code,
      const absl::optional<std::string>& error_page_content,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::PolicyContainerPtr policy_container);

  // The Build*Callback functions below are responsible for building the
  // callbacks for either successful or failed commits.
  // Protected because they need to be called from test overrides.
  mojom::NavigationClient::CommitNavigationCallback
  BuildCommitNavigationCallback(NavigationRequest* navigation_request);
  mojom::NavigationClient::CommitFailedNavigationCallback
  BuildCommitFailedNavigationCallback(NavigationRequest* navigation_request);

  // Protected / virtual so it can be overridden by tests.
  // If `for_legacy` is true, the beforeunload handler is not actually present,
  // nor required to run. In this case the renderer is not notified, but
  // PostTask() is used. PostTask() is used because synchronously proceeding
  // with navigation could lead to reentrancy problems. In particular, there
  // are tests and android WebView using NavigationThrottles to navigate from
  // WillStartRequest(). If PostTask() is not used, then CHECKs would trigger
  // in a NavigationController. See https://crbug.com/365039 for more details.
  virtual void SendBeforeUnload(bool is_reload,
                                base::WeakPtr<RenderFrameHostImpl> impl,
                                bool for_legacy);

 private:
  friend class RenderFrameHostPermissionsPolicyTest;
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;
  friend class TextInputTestLocalFrame;
  friend class WebContentsSplitCacheBrowserTest;
  friend class RenderFrameHostManagerUnloadBrowserTest;
  friend class WebBluetoothServiceImplBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(NavigatorTest, TwoNavigationsRacingCommit);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           SubframeShowsDialogWhenMainFrameNavigates);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           TimerNotRestartedBySecondDialog);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           ComputeSiteForCookiesParentNavigatedAway);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           CheckIsCurrentBeforeAndAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           FindImmediateLocalRoots);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest,
                           HasCommittedAnyNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest, GetUkmSourceIds);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CreateRenderViewAfterProcessKillAndClosedProxy);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, DontSelectInvalidFiles);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreSubframeFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RenderViewInitAfterNewProxyAndProcessKill);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           UnloadPushStateOnCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           WebUIJavascriptDisallowedAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, LastCommittedOrigin);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostManagerUnloadBrowserTest,
      PendingDeleteRFHProcessShutdownDoesNotRemoveSubframes);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderViewHost);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderWidgetHost);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           BindToWebUIFromWebViaMojo);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostIsNotReusedAfterDelayedUnloadACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostStaysActiveWithLateUnloadACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LoadEventForwardingWhilePendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ContextMenuAfterCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ActiveSandboxFlagsRetainedAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LastCommittedURLRetainedAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderFrameProxyNotRecreatedDuringProcessShutdown);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadACKArrivesPriorToProcessShutdownRequest);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameUnload);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, UnloadHandlerSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, Unload_ABAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadNestedPendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, PartialUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           PendingDeletionCheckCompletedOnSubtree);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           DetachedIframeUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionABC);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerSameProcess);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerCrossProcess);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessSSLBrowserTest,
                           UnloadHandlersArePowerful);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessSSLBrowserTest,
                           UnloadHandlersArePowerfulGrandChild);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplTest, ExpectedMainWorldOrigin);
  FRIEND_TEST_ALL_PREFIXES(DocumentUserDataTest, CheckInPendingDeletionState);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest, FrozenAndUnfrozenIPC);

  class SubresourceLoaderFactoriesConfig;

  enum class FencedFrameStatus {
    kNotNestedInFencedFrame,
    kFencedFrameRoot,
    kIframeNestedWithinFencedFrame
  };

  FrameTreeNode* GetSibling(int relative_offset) const;

  FrameTreeNode* FindAndVerifyChildInternal(
      RenderFrameHostOrProxy child_frame_or_proxy,
      bad_message::BadMessageReason reason);

  // IPC Message handlers.
  void OnForwardResourceTimingToParent(
      const ResourceTimingInfo& resource_timing);
  void OnSetNeedsOcclusionTracking(bool needs_tracking);
  void OnSaveImageFromDataURL(const std::string& url_str);

  // Computes the IsolationInfo for both navigations and subresources.
  //
  // For navigations, |frame_origin| is the origin being navigated to. For
  // subresources, |frame_origin| is the value of |last_committed_origin_|. The
  // boolean `anonymous` specifies whether this resource should be loaded with
  // the restrictions of an anonymous iframe.
  net::IsolationInfo ComputeIsolationInfoInternal(
      const url::Origin& frame_origin,
      net::IsolationInfo::RequestType request_type,
      bool anonymous);

  // Returns whether or not this RenderFrameHost is a descendant of |ancestor|.
  // This is equivalent to check that |ancestor| is reached by iterating on
  // GetParent().
  // This is a strict relationship, a RenderFrameHost is never an ancestor of
  // itself.
  // This does not consider inner frame trees (i.e. not accounting for fenced
  // frames, portals or GuestView).
  bool IsDescendantOfWithinFrameTree(RenderFrameHostImpl* ancestor);

  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override;
  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      CreatePortalCallback callback) override;
  void AdoptPortal(const blink::PortalToken& portal_token,
                   AdoptPortalCallback callback) override;
  void CreateFencedFrame(
      mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
          pending_receiver,
      CreateFencedFrameCallback callback) override;
  void GetKeepAliveHandleFactory(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver)
      override;
  void DidCommitProvisionalLoad(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params)
      override;
  void CreateChildFrame(
      int new_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const blink::FramePolicy& frame_policy,
      blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
      blink::FrameOwnerElementType owner_type) override;
  void DidCommitSameDocumentNavigation(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params)
      override;
  void DidOpenDocumentInputStream(const GURL& url) override;
  // |initiator_policy_container_host_keep_alive_handle| is needed to
  // ensure that the PolicyContainerHost of the initiator RenderFrameHost
  // can still be retrieved even if the RenderFrameHost has been deleted in
  // between.
  void BeginNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::PolicyContainerHostKeepAliveHandle>
          initiator_policy_container_host_keep_alive_handle) override;
  void SubresourceResponseStarted(const GURL& url,
                                  net::CertStatus cert_status) override;
  void ResourceLoadComplete(
      blink::mojom::ResourceLoadInfoPtr resource_load_info) override;
  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override;
  void CancelInitialHistoryLoad() override;
  void DidUpdatePreferredColorScheme(
      blink::mojom::PreferredColorScheme preferred_color_scheme) override;
  void DidInferColorScheme(
      blink::mojom::PreferredColorScheme color_scheme) override;
  void UpdateEncoding(const std::string& encoding) override;
  void UpdateState(const blink::PageState& state) override;
  void OpenURL(blink::mojom::OpenURLParamsPtr params) override;
  void DidStopLoading() override;

  friend class RenderAccessibilityHost;
  void HandleAXEvents(const ui::AXTreeID& tree_id,
                      mojom::AXUpdatesAndEventsPtr updates_and_events,
                      int32_t reset_token);
  void HandleAXLocationChanges(const ui::AXTreeID& tree_id,
                               std::vector<mojom::LocationChangesPtr> changes);

  // mojom::DomAutomationControllerHost:
  void DomOperationResponse(const std::string& json_string) override;

  // network::mojom::CookieAccessObserver
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

#if BUILDFLAG(ENABLE_PLUGINS)
  // mojom::PepperHost overrides:
  void InstanceCreated(
      int32_t instance_id,
      mojo::PendingAssociatedRemote<mojom::PepperPluginInstance> instance,
      mojo::PendingAssociatedReceiver<mojom::PepperPluginInstanceHost> host)
      override;
  void BindHungDetectorHost(
      mojo::PendingReceiver<mojom::PepperHungDetectorHost> hung_host,
      int32_t plugin_child_id,
      const base::FilePath& path) override;
  void GetPluginInfo(const GURL& url,
                     const std::string& mime_type,
                     GetPluginInfoCallback callback) override;
  void DidCreateInProcessInstance(int32_t instance,
                                  int32_t render_frame_id,
                                  const GURL& document_url,
                                  const GURL& plugin_url) override;
  void DidDeleteInProcessInstance(int32_t instance) override;
  void DidCreateOutOfProcessPepperInstance(
      int32_t plugin_child_id,
      int32_t pp_instance,
      bool is_external,
      int32_t render_frame_id,
      const GURL& document_url,
      const GURL& plugin_url,
      bool is_priviledged_context,
      DidCreateOutOfProcessPepperInstanceCallback callback) override;
  void DidDeleteOutOfProcessPepperInstance(int32_t plugin_child_id,
                                           int32_t pp_instance,
                                           bool is_external) override;
  void OpenChannelToPepperPlugin(
      const url::Origin& embedder_origin,
      const base::FilePath& path,
      const absl::optional<url::Origin>& origin_lock,
      OpenChannelToPepperPluginCallback callback) override;

  // mojom::PepperHungDetectorHost overrides:
  void PluginHung(bool is_hung) override;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  // Resets any waiting state of this RenderFrameHost that is no longer
  // relevant.
  void ResetWaitingState();

  // Returns whether the given origin and URL is allowed to commit in the
  // current RenderFrameHost. The |url| is used to ensure it matches the origin
  // in cases where it is applicable. This is a more conservative check than
  // RenderProcessHost::FilterURL, since it will be used to kill processes that
  // commit unauthorized origins.
  CanCommitStatus CanCommitOriginAndUrl(const url::Origin& origin,
                                        const GURL& url,
                                        bool is_same_document_navigation,
                                        bool is_pdf,
                                        bool is_sandboxed);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Returns whether the current RenderProcessHost has read access to all the
  // files reported in |state|.
  bool CanAccessFilesOfPageState(const blink::PageState& state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |validated_state|.  It is important that the PageState has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromPageState(const blink::PageState& validated_state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |body|.  It is important that the ResourceRequestBody has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromResourceRequestBody(
      const network::ResourceRequestBody& body);

  void UpdatePermissionsForNavigation(NavigationRequest* request);

  // Returns true if there is an active transient fullscreen allowance for the
  // Window Placement feature (i.e. on screen configuration changes).
  bool WindowPlacementAllowsFullscreen();

  // Returns the latest NavigationRequest that has resulted in sending a Commit
  // IPC to the renderer process that hasn't yet been acked by the DidCommit IPC
  // from the renderer process.  Returns null if no such NavigationRequest
  // exists.
  NavigationRequest* FindLatestNavigationRequestThatIsStillCommitting();

  // Creates URLLoaderFactoryParams for main world of |this|, either based on
  // the |navigation_request|, or (if |navigation_request| is null) on the last
  // committed navigation.
  network::mojom::URLLoaderFactoryParamsPtr
  CreateURLLoaderFactoryParamsForMainWorld(
      const SubresourceLoaderFactoriesConfig& config,
      base::StringPiece debug_tag);

  // Like CreateNetworkServiceDefaultFactoryInternal but also sets up a
  // connection error handler to detect and recover from NetworkService
  // crashes.
  bool CreateNetworkServiceDefaultFactoryAndObserve(
      network::mojom::URLLoaderFactoryParamsPtr params,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // Asks an appropriate `NetworkService` to create a new URLLoaderFactory with
  // the given `params` and binds the factory to the `default_factory_receiver`.
  //
  // The callers typically base the `params` on either 1) the current state of
  // `this` RenderFrameHostImpl (e.g. the origin of the initial empty document,
  // or the last committed origin) or 2) a pending NavigationRequest.
  //
  // If this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.  (This indicates that a layer above //content has
  // wrapped `default_factory_receiver` and may inject arbitrary redirects - for
  // example see WebRequestAPI::MaybeProxyURLLoaderFactory.)
  bool CreateNetworkServiceDefaultFactoryInternal(
      network::mojom::URLLoaderFactoryParamsPtr params,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // Lets ContentBrowserClient and devtools_instrumentation wrap the subresource
  // factories before they are sent to a renderer process.
  void WillCreateURLLoaderFactory(
      const url::Origin& request_initiator,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client = nullptr,
      bool* bypass_redirect_checks = nullptr,
      bool* disable_secure_dns = nullptr,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override = nullptr);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  // The checks do not apply to ExecuteJavaScriptInIsolatedWorld, nor to
  // ExecuteJavaScriptForTests.  See also AssertNonSpeculativeFrame method.
  bool CanExecuteJavaScript();

  // Returns the AXTreeID of the parent when the current frame is a child frame
  // (i.e. not a main frame) or when it's an embedded browser plugin guest, or
  // ui::AXTreeIDUnknown() otherwise.
  ui::AXTreeID GetParentAXTreeID();

  // Returns the AXTreeID of the currently focused frame in the frame tree if
  // the current frame is the root frame, or ui::AXTreeIDUnknown otherwise.
  ui::AXTreeID GetFocusedAXTreeID();

  // Returns the AXTreeData associated to the current frame, ensuring that the
  // AXTreeIDs values for the current, parent and focused frames are up to date.
  ui::AXTreeData GetAXTreeData();

  // Callback in response to an accessibility hit test triggered by
  // AccessibilityHitTest.
  void AccessibilityHitTestCallback(
      int action_request_id,
      ax::mojom::Event event_to_fire,
      base::OnceCallback<void(BrowserAccessibilityManager* hit_manager,
                              int hit_node_id)> opt_callback,
      mojom::HitTestResponsePtr hit_test_response);

  // Callback that will be called as a response to the call to the method
  // content::mojom::RenderAccessibility::SnapshotAccessibilityTree(). The
  // |callback| passed will be invoked after the renderer has responded with a
  // standalone snapshot of the accessibility tree as |snapshot|.
  void RequestAXTreeSnapshotCallback(AXTreeSnapshotCallback callback,
                                     const ui::AXTreeUpdate& snapshot);

  // Callback that will be called as a response to the call to the method
  // blink::mojom::LocalFrame::GetSavableResourceLinks(). The |reply| passed
  // will be a nullptr when the url is not the savable URLs or valid.
  void GetSavableResourceLinksCallback(
      blink::mojom::GetSavableResourceLinksReplyPtr reply);

  // Returns the RenderWidgetHostView used for accessibility. For subframes,
  // this function will return the platform view on the main frame; for main
  // frames, it will return the current frame's view.
  RenderWidgetHostViewBase* GetViewForAccessibility();

  // Returns a raw pointer to the last Web Bluetooth Service associated with the
  // frame, or nullptr otherwise. Used for testing purposes only (see
  // |TestRenderFrameHost|).
  WebBluetoothServiceImpl* GetWebBluetoothServiceForTesting();

  // Allows tests to disable the unload event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableUnloadTimerForTesting();

  // Creates a NavigationRequest for synchronous navigation that have committed
  // in the renderer process. Those are:
  // - same-document renderer-initiated navigations.
  // - synchronous about:blank navigations.
  //
  // TODO(clamy): Eventually, this should only be called for same-document
  // renderer-initiated navigations.
  std::unique_ptr<NavigationRequest>
  CreateNavigationRequestForSynchronousRendererCommit(
      const GURL& url,
      const url::Origin& origin,
      blink::mojom::ReferrerPtr referrer,
      const ui::PageTransition& transition,
      bool should_replace_current_entry,
      bool has_user_gesture,
      const std::vector<GURL>& redirects,
      const GURL& original_request_url,
      bool is_same_document,
      bool is_same_document_history_api_navigation);

  // Helper to process the beforeunload completion callback. |proceed| indicates
  // whether the navigation or tab close should be allowed to proceed.  If
  // |treat_as_final_completion_callback| is true, the frame should stop waiting
  // for any further completion callbacks from subframes. Completion callbacks
  // invoked from the renderer set |treat_as_final_completion_callback| to
  // false, whereas a beforeunload timeout sets it to true. See
  // SendBeforeUnload() for details on `for_legacy`.
  void ProcessBeforeUnloadCompleted(
      bool proceed,
      bool treat_as_final_completion_callback,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time,
      bool for_legacy);

  // Find the frame that triggered the beforeunload handler to run in this
  // frame, which might be the frame itself or its ancestor.  This will
  // return the frame that is navigating, or the main frame if beforeunload was
  // triggered by closing the current tab.  It will return null if no
  // beforeunload is currently in progress.
  RenderFrameHostImpl* GetBeforeUnloadInitiator();

  // Called when a particular frame finishes running a beforeunload handler,
  // possibly as part of processing beforeunload for an ancestor frame. In
  // that case, this is called on the ancestor frame that is navigating or
  // closing, and |frame| indicates which beforeunload completion callback has
  // been invoked on. If a beforeunload timeout occurred,
  // |treat_as_final_completion_callback| is set to true.
  // |is_frame_being_destroyed| is set to true if this was called as part of
  // destroying |frame|. See SendBeforeUnload() for details on `for_legacy`.
  void ProcessBeforeUnloadCompletedFromFrame(
      bool proceed,
      bool treat_as_final_completion_callback,
      RenderFrameHostImpl* frame,
      bool is_frame_being_destroyed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time,
      bool for_legacy);

  // Helper function to check whether the current frame and its subframes need
  // to run beforeunload and, if |send_ipc| is true, send all the necessary
  // IPCs for this frame's subtree. If |send_ipc| is false, this only checks
  // whether beforeunload is needed and returns the answer.  |subframes_only|
  // indicates whether to only check subframes of the current frame, and skip
  // the current frame itself.
  bool CheckOrDispatchBeforeUnloadForSubtree(bool subframes_only,
                                             bool send_ipc,
                                             bool is_reload);

  // Called by |beforeunload_timeout_| when the beforeunload timeout fires.
  void BeforeUnloadTimeout();

  // Update this frame's last committed origin.
  void SetLastCommittedOrigin(const url::Origin& origin);

  // Called when a navigation commits successfully to |url|. This will update
  // |last_committed_site_info_| with the SiteInfo corresponding to |url|.
  // Note that this will recompute the SiteInfo from |url| rather than using
  // GetSiteInstance()->GetSiteInfo(), so that |last_committed_site_info_| is
  // always meaningful: e.g., without site isolation, b.com could commit in a
  // SiteInstance for a.com, but this function will still compute the last
  // committed SiteInfo as b.com.  For example, this can be used to track which
  // sites have committed in which process.
  void SetLastCommittedSiteInfo(const GURL& url);

  // Clears any existing policy and constructs a new policy for this frame,
  // based on its parent frame.
  void ResetPermissionsPolicy();

  // Runs |callback| for all the local roots immediately under this frame, i.e.
  // local roots which are under this frame and their first ancestor which is a
  // local root is either this frame or this frame's local root. For instance,
  // in a frame tree such as:
  //                    A0                   //
  //                 /  |   \                //
  //                B   A1   E               //
  //               /   /  \   \              //
  //              D  A2    C   F             //
  // RFHs at nodes B, E, D, C, and F are all local roots in the given frame tree
  // under the root at A0, but only B, C, and E are considered immediate local
  // roots of A0. Note that this will exclude any speculative RFHs.
  void ForEachImmediateLocalRoot(
      const base::RepeatingCallback<void(RenderFrameHostImpl*)>& callback);

  // This is the actual implementation of the various overloads of
  // |ForEachRenderFrameHost|.
  void ForEachRenderFrameHostImpl(FrameIterationCallbackImpl on_frame,
                                  bool include_speculative);

  // Returns the mojom::Frame interface for this frame in the renderer process.
  // May be overridden by friend subclasses for e.g. tests which wish to
  // intercept outgoing messages. May only be called when the RenderFrame in the
  // renderer exists (i.e. RenderFrameCreated() is true).
  mojom::Frame* GetMojomFrameInRenderer();

  // Utility function used to validate potentially harmful parameters sent by
  // the renderer during the commit notification.
  // A return value of true means that the commit should proceed.
  bool ValidateDidCommitParams(NavigationRequest* navigation_request,
                               mojom::DidCommitProvisionalLoadParams* params,
                               bool is_same_document_navigation);
  // Validates whether we can commit |url| and |origin| for a navigation or a
  // document.open() URL update.
  // A return value of true means that the URL & origin can be committed.
  bool ValidateURLAndOrigin(const GURL& url,
                            const url::Origin& origin,
                            bool is_same_document_navigation,
                            NavigationRequest* navigation_request);

  // Updates the site url if the navigation was successful and the page is not
  // an interstitial.
  void UpdateSiteURL(const GURL& url, bool is_error_page);

  // The actual implementation of committing a navigation in the browser
  // process. Called by the DidCommitProvisionalLoad IPC handler.
  void DidCommitNavigation(
      NavigationRequest* committing_navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  // Called when we receive the confirmation that a navigation committed in the
  // renderer. Used by both DidCommitSameDocumentNavigation and
  // DidCommitNavigation.
  // Returns true if the navigation did commit properly, false if the commit
  // state should be restored to its pre-commit value.
  bool DidCommitNavigationInternal(
      std::unique_ptr<NavigationRequest> navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params);

  // Called when we received the confirmation a new document committed in the
  // renderer. It was created from the |navigation|.
  void DidCommitNewDocument(const mojom::DidCommitProvisionalLoadParams& params,
                            NavigationRequest* navigation);

  // Transfers several document properties stored by |navigation_request| into
  // |this|. Helper for DidCommitNewDocument.
  void TakeNewDocumentPropertiesFromNavigation(
      NavigationRequest* navigation_request);

  // Called by the renderer process when it is done processing a same-document
  // commit request.
  void OnSameDocumentCommitProcessed(
      const base::UnguessableToken& navigation_token,
      bool should_replace_current_entry,
      blink::mojom::CommitResult result);

  // Creates URLLoaderFactory objects for |isolated_world_origins|.
  //
  // Properties of the factories (e.g. their client security state) are either
  // based on the |navigation_request|, or (if |navigation_request| is null) on
  // the last committed navigation.
  //
  // TODO(https://crbug.com/1098410): Remove the method below once Chrome
  // Platform Apps are gone.
  blink::PendingURLLoaderFactoryBundle::OriginMap
  CreateURLLoaderFactoriesForIsolatedWorlds(
      const SubresourceLoaderFactoriesConfig& config,
      const base::flat_set<url::Origin>& isolated_world_origins);

  // Based on the termination |status| and |exit_code|, may generate a crash
  // report to be routed to the Reporting API.
  void MaybeGenerateCrashReport(base::TerminationStatus status, int exit_code);

  // Move every child frame into the pending deletion state.
  // For each process, send the command to delete the local subtree, and wait
  // for unload handlers to finish if needed.
  void StartPendingDeletionOnSubtree();

  // This function checks whether a pending deletion frame and all of its
  // subframes have completed running unload handlers. If so, this function
  // destroys this frame. This will happen as soon as...
  // 1) The children in other processes have been deleted.
  // 2) The ack (mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame or
  // mojom::FrameHost::Detach) has been
  //    received. It means this frame in the renderer process is gone.
  void PendingDeletionCheckCompleted();

  // Call |PendingDeletionCheckCompleted| recursively on this frame and its
  // children. This is useful for pruning frames with no unload handlers from
  // this frame's subtree.
  void PendingDeletionCheckCompletedOnSubtree();

  // In this frame and its children, removes every:
  // - NavigationRequest.
  // - Speculative RenderFrameHost.
  void ResetNavigationsForPendingDeletion();

  // Called on an unloading frame when its unload timeout is reached. This
  // immediately deletes the RenderFrameHost.
  void OnUnloadTimeout();

  // Runs interception set up in testing code, if any.
  // Returns true if we should proceed to the Commit callback, false otherwise.
  bool MaybeInterceptCommitCallback(
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params);

  // If this RenderFrameHost is a local root (i.e., either the main frame or a
  // subframe in a different process than its parent), this returns the
  // RenderWidgetHost corresponding to this frame. Otherwise this returns null.
  // See also GetRenderWidgetHost(), which walks up the tree to find the nearest
  // local root.
  // Main frame: RenderWidgetHost is owned by the RenderViewHost.
  // Subframe: RenderWidgetHost is owned by this RenderFrameHost.
  RenderWidgetHostImpl* GetLocalRenderWidgetHost() const;

  // Called after a new document commit. Every children of the previous document
  // are expected to be deleted or at least to be pending deletion waiting for
  // unload completion. A compromised renderer process or bugs can cause the
  // renderer to "forget" to start deletion. In this case the browser deletes
  // them immediately, without waiting for unload completion.
  // https://crbug.com/950625.
  void EnsureDescendantsAreUnloading();

  // Implements AddMessageToConsole() and AddUniqueMessageToConsole().
  void AddMessageToConsoleImpl(blink::mojom::ConsoleMessageLevel level,
                               const std::string& message,
                               bool discard_duplicates);

  // Helper functions for logging crash keys when ValidateDidCommitParams()
  // determines it cannot commit a URL or origin.
  void LogCannotCommitUrlCrashKeys(const GURL& url,
                                   bool is_same_document_navigation,
                                   NavigationRequest* navigation_request);
  void LogCannotCommitOriginCrashKeys(bool is_same_document_navigation,
                                      NavigationRequest* navigation_request);

  // Verifies that browser-calculated and renderer-calculated values for some
  // params in DidCommitProvisionalLoadParams match, to ensure we can completely
  // remove the dependence on the renderer-calculated values. Logs crash keys
  // and dumps them without crashing if some params don't match.
  // See crbug.com/1131832.
  void VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch(
      NavigationRequest* navigation_request,
      const mojom::DidCommitProvisionalLoadParams& params,
      mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params);

  // Common handler for displaying a javascript dialog from the Run*Dialog
  // mojo handlers. This method sets up some initial state before asking the
  // delegate to create a dialog.
  void RunJavaScriptDialog(const std::u16string& message,
                           const std::u16string& default_prompt,
                           JavaScriptDialogType dialog_type,
                           bool disable_third_party_subframe_suppresion,
                           JavaScriptDialogCallback callback);

  // Callback function used to handle the dialog being closed. It will reset
  // the state in the associated RenderFrameHostImpl and call the associated
  // callback when done.
  void JavaScriptDialogClosed(JavaScriptDialogCallback response_callback,
                              bool success,
                              const std::u16string& user_input);

  // See |SetIsXrOverlaySetup()|
  bool HasSeenRecentXrOverlaySetup();

  bool has_unload_handlers() const {
    return has_unload_handler_ || has_pagehide_handler_ ||
           has_visibilitychange_handler_ || do_not_delete_for_testing_;
  }

  // Converts a content-internal RenderFrameHostImpl::LifecycleStateImpl into a
  // coarser-grained RenderFrameHost::LifecycleState which can be exposed
  // outside of content.
  static RenderFrameHost::LifecycleState GetLifecycleStateFromImpl(
      LifecycleStateImpl state);

  void BindReportingObserver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          reporting_observer_receiver);

  // Check the renderer provided sandbox flags matches with what the browser
  // process computed on its own. This triggers DCHECK and DumpWithoutCrashing()
  //
  // TODO(https://crbug.com/1041376) Remove this when we are confident the value
  // computed from the browser is always matching.
  void CheckSandboxFlags();

  // Sets the embedding token corresponding to the document in this
  // RenderFrameHost.
  void SetEmbeddingToken(const base::UnguessableToken& embedding_token);

  // Records a DocumentCreated UKM event. Called when a Document is committed in
  // this frame.
  void RecordDocumentCreatedUkmEvent(const url::Origin& origin,
                                     const ukm::SourceId document_ukm_source_id,
                                     ukm::UkmRecorder* ukm_recorder);

  // Has the RenderFrame been created in the renderer process and not yet been
  // deleted, exited or crashed. See RenderFrameState.
  bool is_render_frame_created() {
    return render_frame_state_ == RenderFrameState::kCreated;
  }

  // Initializes |policy_container_host_|. Constructor helper.
  //
  // |renderer_initiated_creation_of_main_frame| specifies whether this render
  // frame host's creation was initiated by the renderer process, and this is
  // a main frame. See the constructor for more details.
  void InitializePolicyContainerHost(
      bool renderer_initiated_creation_of_main_frame);

  // Sets |policy_container_host_| and associates it with the current frame.
  // |policy_container_host| must not be nullptr.
  void SetPolicyContainerHost(
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // Initializes |private_network_request_policy_|. Constructor helper.
  void InitializePrivateNetworkRequestPolicy();

  // Returns true if this frame requires a proxy to talk to its parent.
  // Note: Using a proxy to talk to a parent does not imply that the parent
  // is in a different process.
  // (e.g. kProcessSharingWithStrictSiteInstances mode uses proxies for frames
  //  that are in the same process.)
  bool RequiresProxyToParent();

  // Increases by one `commit_navigation_sent_counter_`.
  void IncreaseCommitNavigationCounter();

  // Sets the storage key for the last committed document in this
  // RenderFrameHostImpl.
  void SetStorageKey(const blink::StorageKey& storage_key);

  // Check if we should wait for unload handlers when shutting down the
  // renderer.
  bool ShouldWaitForUnloadHandlers() const;

  // Asserts that `this` is not a speculative frame and calls
  // DumpWithoutCrashing otherwise.  This method should be called from
  // RenderFrameHostImpl's methods that require the caller to "be careful" not
  // to call them on a speculative frame.  One such example is
  // JavaScriptExecuteRequestInIsolatedWorld.
  void AssertNonSpeculativeFrame() const;

  // A feature that blocks back/forward cache is used. Count the usage and evict
  // the entry if necessary.
  void OnBackForwardCacheDisablingFeatureUsed(
      BackForwardCacheDisablingFeature feature);

  // A feature that blocks back/forward cache is removed. Update the count of
  // feature usage. This should only be called from
  // |BackForwardCacheDisablingFeatureHandle|.
  void OnBackForwardCacheDisablingFeatureRemoved(
      BackForwardCacheDisablingFeature feature);

  // Create a self-owned receiver that handles incoming BroadcastChannel
  // ConnectToChannel messages from the renderer.
  void CreateBroadcastChannelProvider(
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelProvider>
          receiver);

  perfetto::protos::pbzero::RenderFrameHost::LifecycleState
  LifecycleStateToProto();

  // The RenderViewHost that this RenderFrameHost is associated with.
  //
  // It is kept alive as long as any RenderFrameHosts or RenderFrameProxyHosts
  // are using it.
  //
  // The refcount allows each RenderFrameHostManager to just care about
  // RenderFrameHosts, while ensuring we have a RenderViewHost for each
  // RenderFrameHost.
  //
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  scoped_refptr<RenderViewHostImpl> render_view_host_;

  const raw_ptr<RenderFrameHostDelegate> delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // The agent scheduling group this RenderFrameHost is associated with. It is
  // initialized through a call to
  // site_instance_->GetOrCreateAgentSchedulingGroupHost() at RenderFrameHost
  // creation time.
  // This ref is used to avoid recreating the renderer process if it has
  // crashed, since using
  // SiteInstance::GetProcess()/GetOrCreateAgentSchedulingGroupHost() has the
  // side effect of creating the process again if it is gone.
  AgentSchedulingGroupHost& agent_scheduling_group_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  raw_ptr<FrameTree> frame_tree_ = nullptr;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  raw_ptr<FrameTreeNode> frame_tree_node_ = nullptr;

  // Stores all of the state related to each browsing context +
  // BrowsingInstance. This includes proxy hosts, and replication state, and
  // will help facilitate the full removal of references to frame_tree_ and
  // frame_tree_node_ (per crbug.com/1179502).
  // TODO(crbug.com/1270671): make this field const when legacy mode is removed.
  scoped_refptr<BrowsingContextState> browsing_context_state_;

  // The immediate children of this specific frame.
  std::vector<std::unique_ptr<FrameTreeNode>> children_;

  // The active parent RenderFrameHost for this frame, if it is a subframe.
  // Null for the main frame.  This is cached because the parent FrameTreeNode
  // may change its current RenderFrameHost while this child is pending
  // deletion, and GetParent() should never return a different value, even if
  // this RenderFrameHost is on the pending deletion list and the parent
  // FrameTreeNode has changed its current RenderFrameHost.
  const raw_ptr<RenderFrameHostImpl> parent_;

  // Number of times we need to iterate from a RenderFrameHost to its parent
  // until we reach main RenderFrameHost (i.e. one which doesn't have a parent).
  // Note that that means this value is scoped to a given FrameTree and the
  // cases when a FrameTree embeds another FrameTree are not reflected here.
  const unsigned int depth_ = 0u;

  // Tracks this frame's last committed navigation's URL. Note that this will be
  // empty before the first commit in this *RenderFrameHost*, even if the
  // FrameTreeNode has committed before with a different RenderFrameHost.
  // The URL is always set to the "commit URL" (the URL set in
  // DidCommitProvisionalLoadParams) after every committed navigation, and also
  // when the renderer process crashes (where it's reset to empty).
  // Note that the value tracked here might be different than the value for
  // `last_document_url` in RendererURLInfo, which tracks the last "document
  // URL" which might not necessarily come from a committed navigation (e.g.
  // the URL can change due to document.open()) or the same as the URL used in
  // DidCommitProvisionalLoadParams (e.g. loadDataWithBaseURL which uses the
  // "base URL" as the "document URL" but the data URL as the "committed URL").
  GURL last_committed_url_;

  // See comment in the definition of RendererURLInfo.
  RendererURLInfo renderer_url_info_;

  // Track this frame's last committed origin.
  url::Origin last_committed_origin_;

  // The storage key for the last committed document in this
  // RenderFrameHostImpl.
  blink::StorageKey storage_key_;

  // The policy to apply to private network requests issued by the last
  // committed document. Set to a default value until a document commits for the
  // first time. The default value depends on whether the
  // |BlockInsecurePrivateNetworkRequests| feature is enabled, see constructor.
  //
  // This property normally depends on the last committed origin and the state
  // of |ContentBrowserClient| at the time the navigation committed. Due to the
  // fact that this is based on the origin computed by the browser process in
  // |NavigationRequest|, whereas |last_commited_origin_| is computed by the
  // renderer process (see crbug.com/888079), there can be rare discrepancies.
  //
  // TODO(https://crbug.com/888079): Simplify the above comment when the
  // behavior it explains is fixed.
  network::mojom::PrivateNetworkRequestPolicy private_network_request_policy_ =
      network::mojom::PrivateNetworkRequestPolicy::kBlock;

  // Track the SiteInfo of the last site we committed successfully, as obtained
  // from SiteInstanceImpl::GetSiteInfoForURL().
  SiteInfo last_committed_site_info_;

  // The most recent non-error URL to commit in this frame.
  // TODO(clamy): Remove this in favor of GetLastCommittedURL().
  // See https://crbug.com/588314.
  GURL last_successful_url_;

  // The http method of the last committed navigation.
  std::string last_http_method_ = "GET";

  // The http status code of the last committed navigation.
  int last_http_status_code_ = 0;

  // The POST ID of the last committed navigation.
  int64_t last_post_id_ = 0;

  // Whether the last committed navigation's CommonNavigationParams'
  // `has_user_gesture` is true or not. Note that this is just the cached value
  // of what happened during the last navigation, and does not reflect the
  // user activation state of this RenderFrameHost. To get the current/live user
  // activation state, get the value from FrameTreeNode's
  // HasStickyUserActivation() or HasTransientUserActivation() instead.
  bool last_navigation_started_with_transient_activation_ = false;

  // Whether the last committed navigation is to an error page.
  bool is_error_page_ = false;

  // Local root subframes directly own their RenderWidgetHost.
  // Please see comments about the GetLocalRenderWidgetHost() function.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  std::unique_ptr<RenderWidgetHostImpl> owned_render_widget_host_;

  const int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame and
  // thus pending deletion.
  bool is_waiting_for_unload_ack_ = false;

  // Tracks the creation state of the RenderFrame in renderer process for this
  // RenderFrameHost.
  enum class RenderFrameState {
    // A RenderFrame has never been created for this RenderFrameHost. The next
    // state will be kCreated or kDeleted.
    kNeverCreated = 0,
    // A RenderFrame has been created in the renderer and is still in that
    // state. The next state will be kDeleting.
    kCreated,
    // A RenderFrame has either
    // - been cleanly deleted
    // - its renderer process has exited or crashed
    // We will call observers of RenderFrameDeleted in this state and this
    // allows us to CHECK if an observer causes us to attempt to change state
    // during deletion. See https://crbug.com/1146573. The next state will
    // be kDeleted and we will move to that before exiting RenderFrameDeleted.
    kDeleting,
    // A RenderFrame has either
    // - been cleanly deleted
    // - its renderer process has exited or crashed
    // The next state may be kCreated if the RenderFrameHost is being reused
    // after a crash or the RenderFrameHost may be destroyed.
    kDeleted,
  };
  RenderFrameState render_frame_state_ = RenderFrameState::kNeverCreated;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_completion_ = false;

  // Valid only when |is_waiting_for_beforeunload_completion_| is true. This
  // indicates whether a subsequent request to launch a modal dialog should be
  // honored or whether it should implicitly cause the unload to be canceled.
  bool beforeunload_dialog_request_cancels_unload_ = false;

  // Valid only when is_waiting_for_beforeunload_completion_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true).
  bool unload_ack_is_for_navigation_ = false;

  // The timeout monitor that runs from when the beforeunload is started in
  // DispatchBeforeUnload() until either the render process invokes the
  // respective completion callback (ProcessBeforeUnloadCompleted()), or until
  // the timeout triggers.
  std::unique_ptr<TimeoutMonitor> beforeunload_timeout_;

  // The delay to use for the beforeunload timeout monitor above.
  base::TimeDelta beforeunload_timeout_delay_;

  // When this frame is asked to execute beforeunload, this maintains a list of
  // frames that need beforeunload completion callbacks to be invoked on.  This
  // may include this frame and/or its descendant frames.  This excludes frames
  // that don't have beforeunload handlers defined.
  //
  // TODO(alexmos): For now, this always includes the navigating frame.  Make
  // this include the navigating frame only if it has a beforeunload handler
  // defined.
  std::set<RenderFrameHostImpl*> beforeunload_pending_replies_;

  // During beforeunload, keeps track whether a dialog has already been shown.
  // Used to enforce at most one dialog per navigation.  This is tracked on the
  // frame that is being navigated, and not on any of its subframes that might
  // have triggered a dialog.
  bool has_shown_beforeunload_dialog_ = false;

  // Returns whether the tab was previously discarded.
  // This is passed to CommitNavigationParams in NavigationRequest.
  bool was_discarded_ = false;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not.
  bool is_loading_ = false;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_ = 0;

  // Used to clean up this RFH when the unload event is taking too long to
  // execute. May be null in tests.
  std::unique_ptr<TimeoutMonitor> unload_event_monitor_timeout_;

  // GeolocationService which provides Geolocation.
  std::unique_ptr<GeolocationServiceImpl> geolocation_service_;

  // IdleManager which provides Idle status.
  std::unique_ptr<IdleManagerImpl> idle_manager_;

  // SensorProvider proxy which acts as a gatekeeper to the real SensorProvider.
  std::unique_ptr<SensorProviderProxyImpl> sensor_provider_proxy_;

  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_registry_;

  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_ = 0;

  // A count of the number of times we received an unexpected fatal
  // accessibility error and needed to reset accessibility, so we don't keep
  // trying to reset forever.
  int accessibility_fatal_error_count_ = 0;

  // The last AXTreeData for this frame received from the RenderFrame.
  ui::AXTreeData ax_tree_data_;

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
#if BUILDFLAG(IS_ANDROID)
  base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>
      smart_clip_callbacks_;
#endif  // BUILDFLAG(IS_ANDROID)

  // Callback when an event is received, for testing.
  AccessibilityCallbackForTesting accessibility_testing_callback_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_ = false;

  // Remotes must be reset in TearDownMojoConnection().
  // Holder of Mojo connection with ImageDownloader service in Blink.
  mojo::Remote<blink::mojom::ImageDownloader> mojo_image_downloader_;

  // Holder of Mojo connection with FindInPage service in Blink.
  mojo::AssociatedRemote<blink::mojom::FindInPage> find_in_page_;

  // Holder of Mojo connection with the LocalFrame in Blink.
  mojo::AssociatedRemote<blink::mojom::LocalFrame> local_frame_;

  // Holder of Mojo connection with the LocalMainFrame in Blink. This
  // remote will be valid when the frame is the active main frame.
  mojo::AssociatedRemote<blink::mojom::LocalMainFrame> local_main_frame_;

  // Holder of Mojo connection with the HighPriorityLocalFrame in blink.
  mojo::Remote<blink::mojom::HighPriorityLocalFrame> high_priority_local_frame_;

  // Holds the cross-document NavigationRequests that are waiting to commit.
  // These are navigations that have passed ReadyToCommit stage and are waiting
  // for a matching commit IPC.
  std::map<NavigationRequest*, std::unique_ptr<NavigationRequest>>
      navigation_requests_;

  // Holds same-document NavigationRequests while waiting for the navigations
  // to commit.
  // TODO(https://crbug.com/1133115): Use the NavigationRequest as key once
  // NavigationRequests are bound to same-document DidCommit callbacks,
  // similar to |navigation_requests_| above.
  base::flat_map<base::UnguessableToken, std::unique_ptr<NavigationRequest>>
      same_document_navigation_requests_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_ = WebUI::kNoWebUI;

  // If true, then the RenderFrame has selected text.
  bool has_selection_ = false;

  // If true, then this RenderFrame has one or more audio streams with audible
  // signal. If false, all audio streams are currently silent (or there are no
  // audio streams).
  bool is_audible_ = false;

  // If true, then this RenderFrameHost is waiting to update its
  // LifecycleStateImpl. Happens when the old RenderFrameHost is waiting to
  // either enter BackForwardCache or PendingDeletion. In this case, the old
  // RenderFrameHost's lifecycle state remains in kActive. During this period,
  // the RenderFrameHost is no longer the current one. The flag is again
  // updated once the lifecycle state changes.
  //
  // TODO(https://crbug.com/1177198): Remove this bool and refactor
  // RenderFrameHostManager::CommitPending() to avoid having a time window where
  // we don't know what the old RenderFrameHost's next lifecycle state should
  // be.
  bool has_pending_lifecycle_state_update_ = false;

  // Used for tracking the latest size of the RenderFrame.
  absl::optional<gfx::Size> frame_size_;

  // This boolean indicates whether the RenderFrame has committed *any*
  // navigation or not. Starts off false and is set to true for the lifetime of
  // the RenderFrame when the first CommitNavigation message is sent to the
  // RenderFrame. It is reset after a renderer process crash.
  bool has_committed_any_navigation_ = false;
  bool must_be_replaced_ = false;

  // Counts the number of times the associated renderer process has exited.
  // This is used to track problematic RenderFrameHost reuse.
  // TODO(https://crbug.com/1172882): Remove once enough data has been
  // collected.
  int renderer_exit_count_ = 0;

  // Receivers must be reset in TearDownMojoConnection().
  mojo::AssociatedReceiver<mojom::FrameHost> frame_host_associated_receiver_{
      this};
  mojo::AssociatedReceiver<blink::mojom::BackForwardCacheControllerHost>
      back_forward_cache_controller_host_associated_receiver_{this};
  mojo::AssociatedRemote<mojom::Frame> frame_;
  mojo::AssociatedRemote<mojom::FrameBindingsControl> frame_bindings_control_;
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>
      local_frame_host_receiver_{this};
  // Should only be bound when the frame is a swapped in main frame.
  mojo::AssociatedReceiver<blink::mojom::LocalMainFrameHost>
      local_main_frame_host_receiver_{this};

  // If this is true then this main-frame object was created in response to a
  // renderer initiated request. Init() will be called when the renderer wants
  // the frame to become visible and to perform navigation requests. Until then
  // navigation requests should be queued.
  bool waiting_for_init_;

  // If true then this frame's document has a focused element which is editable.
  bool has_focused_editable_element_ = false;

  std::unique_ptr<PendingNavigation> pending_navigate_;

  // Renderer-side states that blocks fast shutdown of the frame.
  bool has_before_unload_handler_ = false;
  bool has_unload_handler_ = false;
  bool has_pagehide_handler_ = false;
  bool has_visibilitychange_handler_ = false;

  absl::optional<RenderFrameAudioOutputStreamFactory>
      audio_service_audio_output_stream_factory_;
  absl::optional<RenderFrameAudioInputStreamFactory>
      audio_service_audio_input_stream_factory_;

  // Hosts blink::mojom::PresentationService for the RenderFrame.
  std::unique_ptr<PresentationServiceImpl> presentation_service_;

  // Hosts blink::mojom::FileSystemManager for the RenderFrame.
  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_;

  // Hosts blink::mojom::PushMessaging for the RenderFrame.
  std::unique_ptr<PushMessagingManager> push_messaging_manager_;

  // Hosts blink::mojom::SpeechSynthesis for the RenderFrame.
  std::unique_ptr<SpeechSynthesisImpl> speech_synthesis_impl_;

  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // A bitwise OR of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  int enabled_bindings_ = 0;

  // Parsed permissions policy header. It is parsed from blink, received during
  // DidCommitProvisionalLoad. This is constant during the whole lifetime of
  // this document.
  blink::ParsedPermissionsPolicy permissions_policy_header_;

  // Tracks the permissions policy which has been set on this frame.
  std::unique_ptr<blink::PermissionsPolicy> permissions_policy_;

  // Tracks the document policy which has been set on this frame.
  std::unique_ptr<blink::DocumentPolicy> document_policy_;

#if BUILDFLAG(IS_ANDROID)
  // An InterfaceProvider for Java-implemented interfaces that are scoped to
  // this RenderFrameHost. This provides access to interfaces implemented in
  // Java in the browser process to C++ code in the browser process.
  std::unique_ptr<service_manager::InterfaceProvider> java_interfaces_;
#endif

  // BrowserInterfaceBroker implementation through which this
  // RenderFrameHostImpl exposes document-scoped Mojo services to the currently
  // active document in the corresponding RenderFrame.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `RenderFrameHostImpl*` parameter.
  BrowserInterfaceBrokerImpl<RenderFrameHostImpl, RenderFrameHost*> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // Performs Mojo capability control on this RenderFrameHost when
  // `mojo_binder_policy_applier_` is not null. Mojo binder polices will be
  // applied to interfaces that are registered with BrowserInterfaceBrokerImpl
  // and AssociatedInterfaceRegistry before invoking their binders.
  // Currently, it is non-null pointer only if this RenderFrameHost is being
  // prerendered.
  std::unique_ptr<MojoBinderPolicyApplier> mojo_binder_policy_applier_;

  // IPC-friendly token that represents this host.
  const blink::LocalFrameToken frame_token_;

  // Binding to remote implementation of mojom::RenderAccessibility. Note that
  // this binding is done on-demand (in UpdateAccessibilityMode()) and will only
  // be connected (i.e. bound) to the other endpoint in the renderer while there
  // is an accessibility mode that includes |kWebContents|.
  mojo::AssociatedRemote<mojom::RenderAccessibility> render_accessibility_;

  base::SequenceBound<RenderAccessibilityHost> render_accessibility_host_;

  mojo::AssociatedReceiver<mojom::DomAutomationControllerHost>
      dom_automation_controller_receiver_{this};

#if BUILDFLAG(ENABLE_PLUGINS)
  mojo::AssociatedReceiver<mojom::PepperHost> pepper_host_receiver_{this};
  std::map<int32_t, std::unique_ptr<PepperPluginInstanceHost>>
      pepper_instance_map_;
  struct HungDetectorContext {
    int32_t plugin_child_id;
    const base::FilePath plugin_path;
  };
  mojo::ReceiverSet<mojom::PepperHungDetectorHost, HungDetectorContext>
      pepper_hung_detectors_;
#endif

  KeepAliveHandleFactory keep_alive_handle_factory_;

  // For observing Network Service connection errors only. Will trigger
  // `UpdateSubresourceLoaderFactories()` and push updated factories to
  // `RenderFrame`.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_disconnect_handler_holder_;

  // Whether UpdateSubresourceLoaderFactories should recreate the default
  // URLLoaderFactory when handling a NetworkService crash.
  bool recreate_default_url_loader_factory_after_network_service_crash_ = false;

  // Set of isolated world origins that require a separate URLLoaderFactory
  // (e.g. for handling requests initiated by extension content scripts that
  // require relaxed CORS/CORB rules).
  //
  // TODO(https://crbug.com/1098410): Remove the field below once Chrome
  // Platform Apps are gone.
  base::flat_set<url::Origin>
      isolated_worlds_requiring_separate_url_loader_factory_;

  // Holds the renderer generated ID and global request ID for the main frame
  // request.
  std::pair<int, GlobalRequestID> main_frame_request_ids_;

  // If |ResourceLoadComplete()| is called for the main resource before
  // |DidCommitProvisionalLoad()|, the load info is saved here to call
  // |ResourceLoadComplete()| when |DidCommitProvisionalLoad()| is called. This
  // is necessary so the renderer ID can be mapped to the global ID in
  // |DidCommitProvisionalLoad()|. This situation should only happen when an
  // empty document is loaded.
  blink::mojom::ResourceLoadInfoPtr deferred_main_frame_load_info_;

  // If a subframe failed to finish running its unload handler after
  // |subframe_unload_timeout_| the RenderFrameHost is deleted.
  base::TimeDelta subframe_unload_timeout_;

  // Call OnUnloadTimeout() when the unload timer expires.
  base::OneShotTimer subframe_unload_timer_;

  // BackForwardCache:
  bool is_evicted_from_back_forward_cache_ = false;
  base::OneShotTimer back_forward_cache_eviction_timer_;

  // The reasons given in BackForwardCache::DisableForRenderFrameHost. This is a
  // breakdown of NotRestoredReason::kDisableForRenderFrameHostCalled.
  std::set<BackForwardCache::DisabledReason>
      back_forward_cache_disabled_reasons_;

  // Tracks whether the RenderFrameHost had ever been restored from back/forward
  // cache. Should only be used for debugging purposes for crbug.com/1243541.
  // TODO(https://crbug.com/1243541): Remove this once the bug is fixed.
  bool was_restored_from_back_forward_cache_for_debugging_ = false;

  // Whether proactive BrowsingInstance swap is disabled for this frame or not.
  // Note that even if this is false, proactive BrowsingInstance swap still
  // might not happen on navigations on this frame due to other reasons.
  // Should only be used for testing purposes.
  bool has_test_disabled_proactive_browsing_instance_swap_ = false;

  // This used to re-commit when restoring from the BackForwardCache, with the
  // same params as the original navigation.
  // Note: If BackForwardCache is not enabled, this field is not set.
  mojom::DidCommitProvisionalLoadParamsPtr last_commit_params_;

  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  // Whether the currently committed document is MHTML or not. It is set at
  // commit time based on the MIME type of the NavigationRequest that resulted
  // in the navigation commit. Setting the value should be based only on
  // browser side state as this value is used in security checks.
  bool is_mhtml_document_ = false;

  // Whether the currently committed document is overriding the user agent or
  // not.
  bool is_overriding_user_agent_ = false;

  // Used to intercept DidCommit* calls in tests.
  raw_ptr<CommitCallbackInterceptor> commit_callback_interceptor_ = nullptr;

  // Used to hear about CreateNewPopupWidget calls in tests.
  CreateNewPopupWidgetCallbackForTesting create_new_popup_widget_callback_;

  // Used to hear about UnloadACK calls in tests.
  UnloadACKCallbackForTesting unload_ack_callback_;

  // Mask of the active features tracked by the scheduler used by this frame.
  // This is used only for metrics.
  // See blink::SchedulingPolicy::Feature for the meaning.
  // These values should be cleared on document commit.
  //
  // Some features are tracked in these places:
  //   * `renderer_reported_bfcache_disabling_features_` for features in the
  //      document in the renderer.
  //   * `browser_reported_bfcache_disabling_features_counts_` for the browser
  //      features.
  //   * `DedicatedWorkerHost` for features used in dedicated workers.
  // They are tracked separately, because when the renderer updates the set of
  // features, the browser ones should persist. Also, dedicated workers might be
  // destroyed while their renderers persist.
  BackForwardCacheDisablingFeatures
      renderer_reported_bfcache_disabling_features_;

  // Count the usage of BackForwardCacheDisablingFeature.
  base::flat_map<BackForwardCacheDisablingFeature, int>
      browser_reported_bfcache_disabling_features_counts_;

  // Holds prefetched signed exchanges for SignedExchangeSubresourcePrefetch.
  // They will be passed to the next navigation.
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Isolation information to be used for subresources from the currently
  // committed navigation. Stores both the SiteForCookies and the
  // NetworkIsolationKey.
  //
  // This is specific to a document and should be reset on every cross-document
  // commit.
  //
  // When a new frame is created:
  // 1) If the origin of the creator frame is known, then the new frame inherits
  //    the IsolationInfo from the creator frame, similarly to the last
  //    committed origin (see the SetOriginDependentStateOfNewFrame method).
  // 2) If the origin of the creator frame is not known (e.g. in no-opener case)
  //    then the initial transient isolation info (i.e. the default value below)
  //    will be used.  This will match the opaque origin of such a frame.
  net::IsolationInfo isolation_info_ = net::IsolationInfo::CreateTransient();

  // The factory to load resources from the WebBundle source bound to
  // this file.
  std::unique_ptr<WebBundleHandle> web_bundle_handle_;
  // Subresource Web Bundle information that are kept around across
  // same-document navigations.
  std::unique_ptr<SubresourceWebBundleNavigationInfo>
      subresource_web_bundle_navigation_info_;

  // Tainted once MediaStream access was granted.
  bool was_granted_media_access_ = false;

  // Salt for generating frame-specific media device IDs.
  std::string media_device_id_salt_base_;

  // Keep the list of ServiceWorkerContainerHosts so that they can observe when
  // the frame goes in/out of BackForwardCache.
  // TODO(yuzus): Make this a single pointer. A frame should only have a single
  // container host, but probably during a navigation the old container host is
  // still alive when the new container host is created and added to this
  // vector, and the old container host is destroyed shortly after navigation.
  std::map<std::string, base::WeakPtr<ServiceWorkerContainerHost>>
      service_worker_container_hosts_;
  // Keeps the track of the latest ServiceWorkerContainerHost.
  base::WeakPtr<ServiceWorkerContainerHost> last_committed_service_worker_host_;

  // The portals owned by this frame. |Portal::owner_render_frame_host_| points
  // back to |this|.
  base::flat_set<std::unique_ptr<Portal>, base::UniquePtrComparator> portals_;

  // The fenced frames owned by this document.
  std::vector<std::unique_ptr<FencedFrame>> fenced_frames_;

  // Tracking active features in this frame, for use in figuring out whether
  // or not it can be frozen.
  std::unique_ptr<FeatureObserver> feature_observer_;

  // Optional PeakGpuMemoryTracker, when this frame is the main frame. Created
  // by NavigationRequest, ownership is maintained until the frame has stopped
  // loading. Or newer navigations occur.
  std::unique_ptr<PeakGpuMemoryTracker> loading_mem_tracker_;

  scoped_refptr<WebAuthRequestSecurityChecker>
      webauth_request_security_checker_;

  // The preferred color scheme for this frame.
  blink::mojom::PreferredColorScheme preferred_color_scheme_ =
      blink::mojom::PreferredColorScheme::kLight;

  // Container for arbitrary document-associated feature-specific data. Should
  // be reset when committing a cross-document navigation in this
  // RenderFrameHost. RenderFrameHostImpl stores internal members here
  // directly while consumers of RenderFrameHostImpl should store data via
  // GetDocumentUserData(). Please refer to the description at
  // content/public/browser/document_user_data.h for more details.
  struct DocumentAssociatedData : public base::SupportsUserData {
    explicit DocumentAssociatedData(RenderFrameHostImpl& document);
    ~DocumentAssociatedData() override;
    DocumentAssociatedData(const DocumentAssociatedData&) = delete;
    DocumentAssociatedData& operator=(const DocumentAssociatedData&) = delete;

    // The Page object associated with the main document. It is nullptr for
    // subframes.
    std::unique_ptr<PageImpl> owned_page;

    // Indicates whether `blink::mojom::DidDispatchDOMContentLoadedEvent` was
    // called for this document or not.
    bool dom_content_loaded = false;

    // Prerender2:
    //
    // The URL that `blink.mojom.LocalFrameHost::DidFinishLoad()` passed to
    // DidFinishLoad, nullopt if DidFinishLoad wasn't called for this document
    // or this document is not in prerendering. This is used to defer and
    // dispatch DidFinishLoad notification on prerender activation.
    absl::optional<GURL> pending_did_finish_load_url_for_prerendering;

    // Reporting API:
    //
    // Contains the reporting source token for this document, which will be
    // associated with the reporting endpoint configuration in the network
    // service, as well as with any reports which are queued by this document.
    base::UnguessableToken reporting_source;

    // "Owned" but not with std::unique_ptr, as a DocumentServiceBase is
    // allowed to delete itself directly.
    std::vector<internal::DocumentServiceBase*> services;

    // Produces weak pointers to the hosting RenderFrameHostImpl. This is
    // invalidated whenever DocumentAssociatedData is destroyed, due to
    // RenderFrameHost deletion or cross-document navigation.
    base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory;
  };

  // Reset immediately before a RenderFrameHost is reused for hosting a new
  // document.
  //
  // Note: this is an absl::optional instead of a std::unique_ptr because:
  // 1. it is always allocated
  // 2. `~RenderFrameHostImpl` destroys `document_associated_data_` which
  //    destroys any `DocumentService` objects tracking `this`. Destroying a
  //    `DocumentService` unregisters it from `this`. A std::unique_ptr's
  //    stored pointer value is (intentionally) undefined during destruction
  //    (e.g. it could be nullptr), which would cause unregistration to
  //    dereference a null pointer.
  absl::optional<DocumentAssociatedData> document_associated_data_;

  // Keeps track of the scenario when RenderFrameHostManager::CommitPending is
  // called before the navigation commits. This becomes true if the previous
  // RenderFrameHost is not alive and the speculative RenderFrameHost is
  // committed early (see RenderFrameHostManager::GetFrameHostForNavigation for
  // more details). While |committed_speculative_rfh_before_navigation_commit_|
  // is true the RenderFrameHost which we commit early will be live.
  bool committed_speculative_rfh_before_navigation_commit_ = false;

  // This time is used to record the last WebXR DOM Overlay setup request.
  base::TimeTicks last_xr_overlay_setup_time_;

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;
  CrossOriginOpenerPolicyAccessReportManager coop_access_report_manager_;

  // https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#virtual-browsing-context-group-id
  //
  // Whenever we detect that the enforcement of a report-only COOP policy would
  // have resulted in a BrowsingInstance switch, we assign a new virtual
  // browsing context group ID to the RenderFrameHostImpl that has navigated.
  int virtual_browsing_context_group_;

  // Used to track browsing context group switches that would happen if COOP
  // had a value of same-origin-allow-popups by default.
  int soap_by_default_virtual_browsing_context_group_;

  // Navigation ID for the last committed cross-document non-bfcached navigation
  // in this RenderFrameHost.
  // TODO(crbug.com/936696): Make this const after we have RenderDocument.
  int64_t last_committed_cross_document_navigation_id_ = -1;

  // Tracks the state of |this| RenderFrameHost from the point it is created to
  // when it gets deleted.
  LifecycleStateImpl lifecycle_state_;

  // If true, RenderFrameHost should not be actually deleted and should be left
  // stuck in pending deletion.
  bool do_not_delete_for_testing_ = false;

  // Embedding token for the document in this RenderFrameHost. This differs from
  // |frame_token_| in that |frame_token_| has a lifetime matching that of the
  // corresponding RenderFrameHostImpl, and is intended to be used for IPCs for
  // identifying frames just like routing IDs. |embedding_token_| has a document
  // scoped lifetime and changes on cross-document navigations.
  absl::optional<base::UnguessableToken> embedding_token_;

  // Observers listening to cookie access notifications for the current document
  // in this RenderFrameHost.
  // Note: at the moment this set is not cleared when a new document is created
  // in this RenderFrameHost. This is done because the first observer is created
  // before the navigation actually commits and because the old routing id-based
  // behaved in the same way as well.
  // This problem should go away with RenderDocumentHost in any case.
  // TODO(crbug.com/936696): Remove this warning after the RDH ships.
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;

  // Indicates whether this frame is an outer delegate frame for some other
  // RenderFrameHost. This will be a valid ID if so, and
  // `kFrameTreeNodeInvalidId` otherwise.
  int inner_tree_main_frame_tree_node_id_;

  // Indicates whether navigator.credentials.get({otp: {transport:"sms"}}) has
  // been used on a document (regardless of the outcome).
  // Note that WebOTP is not allowed in iframes for security reasons. i.e. this
  // will not be set in such case which is expected. In addition, since the
  // RenderFrameHost may persist across navigations, we need to reset the bit
  // to make sure that it's used per document.
  bool document_used_web_otp_ = false;

  // The browsing context's required CSP as defined by
  // https://w3c.github.io/webappsec-cspee/#required-csp,
  // stored when the frame commits the navigation.
  network::mojom::ContentSecurityPolicyPtr required_csp_;

  // Whether the current document is loaded inside an anonymous iframe. Updated
  // on every cross-document navigation.
  bool anonymous_ = false;

  // The PolicyContainerHost for the current document, containing security
  // policies that apply to it. It should never be null if the RenderFrameHost
  // is displaying a document. Its lifetime should coincide with the lifetime of
  // the document displayed in the RenderFrameHost. It is overwritten at
  // navigation commit time in DidCommitNewDocument with the PolicyContainerHost
  // of the new document. Never set this directly, but always use
  // SetPolicyContainerHost.
  // Note: Although it is owned through a scoped_refptr, a PolicyContainerHost
  // should not be shared between different owners. The PolicyContainerHost of a
  // RenderFrameHost can be retrieven with PolicyContainerHost::FromFrameToken
  // even after the RenderFrameHost has been deleted, if there exist still some
  // keepalive for it. One keepalive is always held by the LocalFrame's
  // PolicyContainer. Cf. the documentation string of the PolicyContainerHost
  // class for more information.
  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // The current document's HTTP response head. This is used by back-forward
  // cache, for navigating a second time toward the same document.
  network::mojom::URLResponseHeadPtr last_response_head_;

  std::unique_ptr<NavigationEarlyHintsManager> early_hints_manager_;

  // A counter which is incremented by one every time this RenderFrameHost sends
  // a `CommitNavigation` or `CommitFailedNavigation` IPC to the renderer.
  int commit_navigation_sent_counter_ = 0;

  // CodeCacheHost processes requests to fetch / write generated code for
  // JavaScript / WebAssembly resources.
  CodeCacheHostImpl::ReceiverSet code_cache_host_receivers_;

  // Holds the mapping of names to URLs of reporting endpoints for the current
  // document, as parsed from the Reporting-Endpoints response header. This data
  // comes directly from the structured header parser, and does not necessarily
  // represent a valid reporting configuration. This is passed to the network
  // service to set up the actual endpoint configuration once the document load
  // commits.
  base::flat_map<std::string, std::string> reporting_endpoints_;

  // This indicates whether `this` is not nested in a fenced frame, or `this` is
  // associated with a fenced frame root, or `this` is associated with an iframe
  // nested within a fenced frame.
  const FencedFrameStatus fenced_frame_status_;

  // Testing callback run in DidStopLoading() regardless of loading state. This
  // is useful for tests that need to detect when newly created frames finish
  // loading about:blank.
  base::OnceClosure did_stop_loading_callback_;

  // Used when testing to retrieve that last created Web Bluetooth service.
  raw_ptr<WebBluetoothServiceImpl> last_web_bluetooth_service_for_testing_ =
      nullptr;

  BackForwardCacheDisablingFeaturesCallback
      back_forward_cache_disabling_features_callback_for_testing_;

  int check_if_deleted_request_count_ = 0;

  // WeakPtrFactories are the last members, to ensure they are destroyed before
  // all other fields of `this`.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_{this};

  // Unlike `weak_ptr_factory` which only invalidates when `this` is about to be
  // deleted, `render_frame_scoped_weak_ptr_factory_` is invalidated every time
  // the RenderFrame is deleted (for example, if the renderer crashes).
  base::WeakPtrFactory<RenderFrameHostImpl>
      render_frame_scoped_weak_ptr_factory_{this};
};

// Used when DCHECK_STATE_TRANSITION triggers.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& o,
    const RenderFrameHostImpl::LifecycleStateImpl& s);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_IMPL_H_
