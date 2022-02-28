// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/input/input_device_change_observer.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_states.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_switching_observer.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {

class AgentSchedulingGroupHost;
class RenderProcessHost;
class SiteInfo;
class TimeoutMonitor;

// A callback which will be called immediately before EnterBackForwardCache
// starts.
using WillEnterBackForwardCacheCallbackForTesting =
    base::RepeatingCallback<void()>;

// A callback which will be called immediately before sending the
// RendererPreferences information to the renderer.
using WillSendRendererPreferencesCallbackForTesting =
    base::RepeatingCallback<void(const blink::RendererPreferences&)>;

// This implements the RenderViewHost interface that is exposed to
// embedders of content, and adds things only visible to content.
//
// The exact API of this object needs to be more thoroughly designed. Right
// now it mimics what WebContentsImpl exposed, which is a fairly large API and
// may contain things that are not relevant to a common subset of views. See
// also the comment in render_view_host_delegate.h about the size and scope of
// the delegate API.
//
// Right now, the concept of page navigation (both top level and frame) exists
// in the WebContentsImpl still, so if you instantiate one of these elsewhere,
// you will not be able to traverse pages back and forward. We need to determine
// if we want to bring that and other functionality down into this object so it
// can be shared by others.
//
// DEPRECATED: RenderViewHostImpl is being removed as part of the SiteIsolation
// project. New code should not be added here, but to either RenderFrameHostImpl
// (if frame specific) or PageImpl (if page specific).
//
// For context, please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewHostImpl
    : public RenderViewHost,
      public RenderWidgetHostOwnerDelegate,
      public RenderProcessHostObserver,
      public ui::GpuSwitchingObserver,
      public IPC::Listener,
      public base::RefCounted<RenderViewHostImpl> {
 public:
  static constexpr int kUnloadTimeoutInMSec = 500;

  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int process_id, int routing_id);

  // Convenience function, just like RenderViewHost::From.
  static RenderViewHostImpl* From(RenderWidgetHost* rwh);

  static void GetPlatformSpecificPrefs(blink::RendererPreferences* prefs);

  // Checks whether any RenderViewHostImpl instance associated with a given
  // process is not currently in the back-forward cache.
  // TODO(https://crbug.com/1125996): Remove once a well-behaved frozen
  // RenderFrame never send IPCs messages, even if there are active pages in the
  // process.
  static bool HasNonBackForwardCachedInstancesForProcess(
      RenderProcessHost* process);

  RenderViewHostImpl(FrameTree* frame_tree,
                     SiteInstance* instance,
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t routing_id,
                     int32_t main_frame_routing_id,
                     bool swapped_out,
                     bool has_initialized_audio_host);

  RenderViewHostImpl(const RenderViewHostImpl&) = delete;
  RenderViewHostImpl& operator=(const RenderViewHostImpl&) = delete;

  // RenderViewHost implementation.
  RenderWidgetHostImpl* GetWidget() override;
  RenderProcessHost* GetProcess() override;
  int GetRoutingID() override;
  void EnablePreferredSizeMode() override;
  bool IsRenderViewLive() override;
  void WriteIntoTrace(perfetto::TracedValue context) override;

  void SendWebPreferencesToRenderer();
  void SendRendererPreferencesToRenderer(
      const blink::RendererPreferences& preferences);

  // RenderProcessHostObserver implementation
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // GpuSwitchingObserver implementation.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost.
  // |opener_route_id| parameter indicates which RenderView created this
  //   (MSG_ROUTING_NONE if none).
  // |window_was_opened_by_another_window| is true if this top-level frame was
  // created by another window, as opposed to independently created (through
  // the browser UI, etc). This is true even when the window is opened with
  // "noopener", and even if the opener has been closed since.
  // |proxy_route_id| is only used when creating a RenderView in an inactive
  //   state.
  virtual bool CreateRenderView(
      const absl::optional<blink::FrameToken>& opener_frame_token,
      int proxy_route_id,
      bool window_was_opened_by_another_window);

  RenderViewHostDelegate* GetDelegate();

  // Tracks whether this RenderViewHost is in an active state (rather than
  // pending unload or unloaded), according to its main frame
  // RenderFrameHost.
  bool is_active() const { return main_frame_routing_id_ != MSG_ROUTING_NONE; }

  // TODO(creis): Remove as part of http://crbug.com/418265.
  bool is_waiting_for_page_close_completion() const {
    return is_waiting_for_page_close_completion_;
  }

  // Called when the RenderView in the renderer process has been created, at
  // which point IsRenderViewLive() becomes true, and the mojo connections to
  // the renderer process for this view now exist.
  void RenderViewCreated(RenderFrameHostImpl* local_main_frame);

  // Returns the main RenderFrameHostImpl associated with this RenderViewHost or
  // null if it doesn't exist. It's null if the main frame is represented in
  // this RenderViewHost by RenderFrameProxyHost (from Blink perspective,
  // blink::Page's main blink::Frame is remote).
  RenderFrameHostImpl* GetMainRenderFrameHost();

  // Returns the `AgentSchedulingGroupHost` this view is associated with (via
  // the widget).
  AgentSchedulingGroupHost& GetAgentSchedulingGroup();

  // Tells the renderer process to request a page-scale animation based on the
  // specified point/rect.
  void AnimateDoubleTapZoom(const gfx::Point& point, const gfx::Rect& rect);

  // Tells the renderer process to run the page's unload handler.
  // A completion callback is invoked by the renderer when the handler
  // execution completes.
  void ClosePage();

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Requests a page-scale animation based on the specified rect.
  void ZoomToFindInPageRect(const gfx::Rect& rect_to_zoom);

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  bool SuddenTerminationAllowed();
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // Send RenderViewReady to observers once the process is launched, but not
  // re-entrantly.
  void PostRenderViewReady();

  // Passes current web preferences to the renderer after recomputing all of
  // them, including the slow-to-compute hardware preferences.
  // (WebContents::OnWebPreferencesChanged is a faster alternate that avoids
  // slow recomputations.)
  void OnHardwareConfigurationChanged();

  // Sets the routing id for the main frame. When set to MSG_ROUTING_NONE, the
  // view is not considered active.
  void SetMainFrameRoutingId(int routing_id);

  // Called when the RenderFrameHostImpls/RenderFrameProxyHosts that own this
  // RenderViewHost enter the BackForwardCache.
  void EnterBackForwardCache();

  // Indicates whether or not |this| has received an acknowledgement from
  // renderer that it has enered BackForwardCache.
  bool DidReceiveBackForwardCacheAck();

  // Called when the RenderFrameHostImpls/RenderFrameProxyHosts that own this
  // RenderViewHost leave the BackForwardCache. This occurs immediately before a
  // restored document is committed.
  // |page_restore_params| includes information that is needed by the page after
  // getting restored, which includes the latest history information (offset,
  // length) and the timestamp corresponding to the start of the back-forward
  // cached navigation, which would be communicated to the page to allow it to
  // record the latency of this navigation.
  // TODO(https://crbug.com/1234634): Remove
  // restoring_main_frame_from_back_forward_cache.
  void LeaveBackForwardCache(
      blink::mojom::PageRestoreParamsPtr page_restore_params,
      bool restoring_main_frame_from_back_forward_cache);

  bool is_in_back_forward_cache() const { return is_in_back_forward_cache_; }

  void ActivatePrerenderedPage(base::TimeTicks activation_start,
                               base::OnceClosure callback);

  void SetFrameTreeVisibility(blink::mojom::PageVisibilityState visibility);

  void SetIsFrozen(bool frozen);
  void OnBackForwardCacheTimeout();
  void MaybeEvictFromBackForwardCache();
  void EnforceBackForwardCacheSizeLimit();

  PageLifecycleStateManager* GetPageLifecycleStateManager() {
    return page_lifecycle_state_manager_.get();
  }

  // Called during frame eviction to return all SurfaceIds in the frame tree.
  // Marks all views in the frame tree as evicted.
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction();

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderViewHost() const;

  void SetWillEnterBackForwardCacheCallbackForTesting(
      const WillEnterBackForwardCacheCallbackForTesting& callback);

  void SetWillSendRendererPreferencesCallbackForTesting(
      const WillSendRendererPreferencesCallbackForTesting& callback);

  void BindPageBroadcast(
      mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast>
          page_broadcast);

  // The remote mojom::PageBroadcast interface that is used to send messages to
  // the renderer's blink::WebViewImpl when broadcasting messages to all
  // renderers hosting frames in the frame tree.
  const mojo::AssociatedRemote<blink::mojom::PageBroadcast>&
  GetAssociatedPageBroadcast();

  // Prepares the renderer page to leave the back-forward cache by disabling
  // Javascript eviction. |done_cb| is called upon receipt of the
  // acknowledgement from the renderer that this has actually happened.
  //
  // After |done_cb| is called you can be certain that this renderer will not
  // trigger an eviction of this page.
  void PrepareToLeaveBackForwardCache(base::OnceClosure done_cb);

  // TODO(https://crbug.com/1179502): FrameTree and FrameTreeNode will not be
  // const as with prerenderer activation the page needs to move between
  // FrameTreeNodes and FrameTrees. As it's hard to make sure that all places
  // handle this transition correctly, MPArch will remove references from this
  // class to FrameTree/FrameTreeNode.
  FrameTree* frame_tree() const { return frame_tree_; }
  void SetFrameTree(FrameTree& frame_tree);

  // Write a representation of this object into a trace.
  void WriteIntoTrace(
      perfetto::TracedProto<perfetto::protos::pbzero::RenderViewHost> proto);

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  friend class base::RefCounted<RenderViewHostImpl>;
  ~RenderViewHostImpl() override;

  // RenderWidgetHostOwnerDelegate overrides.
  void RenderWidgetGotFocus() override;
  void RenderWidgetLostFocus() override;
  void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override;
  bool MayRenderWidgetForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) override;
  bool ShouldContributePriorityToProcess() override;
  void SetBackgroundOpaque(bool opaque) override;
  bool IsMainFrameActive() override;
  bool IsNeverComposited() override;
  blink::web_pref::WebPreferences GetWebkitPreferencesForWidget() override;

  // IPC message handlers.
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_rect,
                  bool user_gesture);
  void OnShowWidget(int widget_route_id, const gfx::Rect& initial_rect);
  void OnPasteFromSelectionClipboard();
  void OnTakeFocus(bool reverse);
  void OnFocus();

 private:
  // TODO(nasko): Temporarily friend RenderFrameHostImpl, so we don't duplicate
  // utility functions and state needed in both classes, while we move frame
  // specific code away from this class.
  friend class RenderFrameHostImpl;
  friend class TestRenderViewHost;
  friend class PageLifecycleStateManagerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, BasicRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, RoutingIdSane);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CloseWithPendingWhileUnresponsive);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  std::string ToDebugString() override;

  void RenderViewReady();

  // Called by |close_timeout_| when the page closing timeout fires.
  void ClosePageTimeout();

  void OnPageClosed();

  // TODO(creis): Move to a private namespace on RenderFrameHostImpl.
  // Delay to wait on closing the WebContents for a beforeunload/unload handler
  // to fire.
  static constexpr base::TimeDelta kUnloadTimeout =
      base::Milliseconds(kUnloadTimeoutInMSec);

  // The RenderWidgetHost.
  const std::unique_ptr<RenderWidgetHostImpl> render_widget_host_;

  // Our delegate, which wants to know about changes in the RenderView.
  raw_ptr<RenderViewHostDelegate> delegate_;

  // ID to use when registering/unregistering this object with its FrameTree.
  // This ID is generated by passing a SiteInstance to
  // FrameTree::GetRenderViewHostMapId(). This RenderViewHost may only be reused
  // by frames with SiteInstances that generate an ID that matches this field.
  FrameTree::RenderViewHostMapId render_view_host_map_id_;

  // SiteInfo taken from the SiteInstance passed into the constructor. It is
  // used to determine if this is a guest view and provides information for
  // selecting the session storage namespace for this view.
  //
  // TODO(acolwell): Replace this with StoragePartitionConfig once we no longer
  // need a StoragePartitionId and StoragePartitionConfig to lookup a
  // SessionStorageNamespace.
  SiteInfo site_info_;

  // Routing ID for this RenderViewHost.
  const int routing_id_;

  // Whether the renderer-side RenderView is created. Becomes false when the
  // renderer crashes.
  bool renderer_view_created_ = false;

  // Routing ID for the main frame's RenderFrameHost.
  int main_frame_routing_id_;

  // Set to true when waiting for a blink.mojom.LocalMainFrame.ClosePage()
  // to complete.
  //
  // TODO(creis): Move to RenderFrameHost and RenderWidgetHost.
  // See http://crbug.com/418265.
  bool is_waiting_for_page_close_completion_ = false;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_ = false;

  // The timeout monitor that runs from when the page close is started in
  // ClosePage() until either the render process ACKs the close with an IPC to
  // OnClosePageACK(), or until the timeout triggers and the page is forcibly
  // closed.
  std::unique_ptr<TimeoutMonitor> close_timeout_;

  // This monitors input changes so they can be reflected to the interaction MQ.
  std::unique_ptr<InputDeviceChangeObserver> input_device_change_observer_;

  // This controls the lifecycle change and notify the renderer.
  std::unique_ptr<PageLifecycleStateManager> page_lifecycle_state_manager_;

  bool updating_web_preferences_ = false;

  // BackForwardCache:
  bool is_in_back_forward_cache_ = false;

  WillEnterBackForwardCacheCallbackForTesting
      will_enter_back_forward_cache_callback_for_testing_;

  WillSendRendererPreferencesCallbackForTesting
      will_send_renderer_preferences_callback_for_testing_;

  mojo::AssociatedRemote<blink::mojom::PageBroadcast> page_broadcast_;

  raw_ptr<FrameTree> frame_tree_;

  base::WeakPtrFactory<RenderViewHostImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
