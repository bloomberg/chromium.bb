// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/selection.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/devtools/devtools_frame_metadata.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/input/stylus_text_selector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/delegated_frame_host_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace ui {
class MotionEventAndroid;
class OverscrollRefreshHandler;
struct DidOverscrollParams;
}

namespace content {
class GestureListenerManager;
class ImeAdapterAndroid;
class OverscrollControllerAndroid;
class SelectionPopupController;
class SynchronousCompositorHost;
class SynchronousCompositorClient;
class TextSuggestionHostAndroid;
class TouchSelectionControllerClientManagerAndroid;
class WebContentsAccessibilityAndroid;
struct NativeWebKeyboardEvent;
struct ContextMenuParams;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public StylusTextSelectorClient,
      public content::TextInputManager::Observer,
      public ui::EventHandlerAndroid,
      public ui::GestureProviderClient,
      public ui::TouchSelectionControllerClient,
      public ui::ViewAndroidObserver,
      public ui::WindowAndroidObserver,
      public viz::BeginFrameObserver {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              gfx::NativeView parent_native_view);
  ~RenderWidgetHostViewAndroid() override;

  // Interface used to observe the destruction of a RenderWidgetHostViewAndroid.
  class DestructionObserver {
   public:
    virtual void RenderWidgetHostViewDestroyed(
        RenderWidgetHostViewAndroid* rwhva) = 0;

   protected:
    virtual ~DestructionObserver() {}
  };

  void AddDestructionObserver(DestructionObserver* connector);
  void RemoveDestructionObserver(DestructionObserver* connector);

  ui::TouchSelectionController* touch_selection_controller() {
    return touch_selection_controller_.get();
  }

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() override;
  gfx::Size GetVisibleViewportSize() override;
  void SetInsets(const gfx::Insets& insets) override;
  gfx::Size GetCompositorViewportPixelSize() override;
  bool IsSurfaceAvailableForCopy() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  uint32_t GetCaptureSequenceNumber() const override;
  int GetMouseWheelMinimumGranularity() const override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;
  void RenderProcessGone() override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  InputEventAckState FilterChildGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  bool OnUnconsumedKeyboardEventAck(
      const NativeWebKeyboardEventWithLatencyInfo& event) override;
  void FallbackCursorModeLockCursor(bool left,
                                    bool right,
                                    bool up,
                                    bool down) override;
  void FallbackCursorModeSetCursorVisibility(bool visible) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate,
      bool for_root_frame) override;
  bool LockMouse(bool request_unadjusted_movement) override;
  void UnlockMouse() override;
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) override;
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void ResetFallbackToFirstNavigationSurface() override;
  bool RequestRepaintForTesting() override;
  void SetIsInVR(bool is_in_vr) override;
  bool IsInVR() const override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void DidStopFlinging() override;
  void OnInterstitialPageAttached() override;
  void OnInterstitialPageGoingAway() override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  void OnDidNavigateMainFrameToNewPage() override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;
  void SetWantsAnimateOnlyBeginFrames() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point) override;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation()
      const override;
  void OnRenderWidgetInit() override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  void OnSynchronizedDisplayPropertiesChanged() override;
  base::Optional<SkColor> GetBackgroundColor() override;
  void DidNavigate() override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void GetScreenInfo(ScreenInfo* screen_info) override;
  void CancelActiveTouches() override;

  // ui::EventHandlerAndroid implementation.
  bool OnTouchEvent(const ui::MotionEventAndroid& m) override;
  bool OnMouseEvent(const ui::MotionEventAndroid& m) override;
  bool OnMouseWheelEvent(const ui::MotionEventAndroid& event) override;
  bool OnGestureEvent(const ui::GestureEventAndroid& event) override;
  void OnPhysicalBackingSizeChanged() override;

  // ui::ViewAndroidObserver implementation:
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;
  bool RequiresDoubleTapGestureEvents() const override;

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override {}
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;
  void OnCursorVisibilityChanged(bool visible) override;
  void OnFallbackCursorModeToggled(bool is_on) override;

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override;
  void OnStylusSelectUpdate(float x, float y) override;
  void OnStylusSelectEnd(float x, float y) override;
  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override;

  // ui::TouchSelectionControllerClient implementation.
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // Used by DelegatedFrameHostClientAndroid.
  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source);
  void DidPresentCompositorFrames(
      const viz::FrameTimingDetailsMap& timing_details);
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources);
  void ReclaimResources(const std::vector<viz::ReturnedResource>& resources);

  // viz::BeginFrameObserver implementation.
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

  // Non-virtual methods
  void UpdateNativeViewTree(gfx::NativeView parent_native_view);

  // Returns the temporary background color of the underlaying document, for
  // example, returns black during screen rotation.
  base::Optional<SkColor> GetCachedBackgroundColor();
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendMouseEvent(const ui::MotionEventAndroid&, int action_button);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);
  bool ShowSelectionMenu(const ContextMenuParams& params);
  void set_ime_adapter(ImeAdapterAndroid* ime_adapter) {
    ime_adapter_android_ = ime_adapter;
  }
  void set_selection_popup_controller(SelectionPopupController* controller) {
    selection_popup_controller_ = controller;
  }
  void set_text_suggestion_host(
      TextSuggestionHostAndroid* text_suggestion_host) {
    text_suggestion_host_ = text_suggestion_host;
  }
  void set_gesture_listener_manager(GestureListenerManager* manager) {
    gesture_listener_manager_ = manager;
  }

  base::WeakPtr<RenderWidgetHostViewAndroid> GetWeakPtrAndroid();

  bool OnTouchHandleEvent(const ui::MotionEvent& event);
  int GetTouchHandleHeight();
  void ResetGestureDetection();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  bool SynchronizeVisualProperties(
      const cc::DeadlinePolicy& deadline_policy,
      const base::Optional<viz::LocalSurfaceIdAllocation>&
          child_local_surface_id_allocation);

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);
  void DismissTextHandles();
  void SetTextHandlesTemporarilyHidden(bool hide_handles);
  void SelectWordAroundCaretAck(bool did_select,
                                int start_adjust,
                                int end_adjust);

  // TODO(ericrk): Ideally we'd reemove |root_scroll_offset| from this function
  // once we have a reliable way to get it through RenderFrameMetadata.
  void FrameTokenChangedForSynchronousCompositor(
      uint32_t frame_token,
      const gfx::ScrollOffset& root_scroll_offset);

  void SetSynchronousCompositorClient(SynchronousCompositorClient* client);

  SynchronousCompositorClient* synchronous_compositor_client() const {
    return synchronous_compositor_client_;
  }

  void OnOverscrollRefreshHandlerAvailable();

  // TextInputManager::Observer overrides.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_change_state) override;
  void OnImeCompositionRangeChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override;
  void OnImeCancelComposition(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;

  ImeAdapterAndroid* ime_adapter_for_testing() { return ime_adapter_android_; }

  ui::TouchSelectionControllerClient*
  GetSelectionControllerClientManagerForTesting();
  void SetSelectionControllerClientForTesting(
      std::unique_ptr<ui::TouchSelectionControllerClient> client);

  void SetOverscrollControllerForTesting(
      ui::OverscrollRefreshHandler* overscroll_refresh_handler);

  void GotFocus();
  void LostFocus();

  // RenderFrameMetadataProvider::Observer
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override;
  void OnRenderFrameMetadataChangedAfterActivation() override;

  void WasEvicted();

  void SetWebContentsAccessibility(
      WebContentsAccessibilityAndroid* web_contents_accessibility);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java
  bool IsReady(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void DismissTextHandles(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Returns an int equivalent to an Optional<SKColor>, with a value of 0
  // indicating SKTransparent for not set.
  jint GetBackgroundColor(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void ShowContextMenuAtTouchHandle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint x,
      jint y);

  // Insets the Visual Viewport's bottom by the amount given.  The adjustment
  // is specified in pixels and should not be negative.  An adjustment of 0
  // returns the Visual Viewport to a non-inset viewport that matches the
  // Layout Viewport.
  void InsetViewportBottom(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint bottom_adjust_px);

  void WriteContentBitmapToDiskAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint width,
      jint height,
      const base::android::JavaParamRef<jstring>& jpath,
      const base::android::JavaParamRef<jobject>& jcallback);

  ui::DelegatedFrameHostAndroid* delegated_frame_host_for_testing() {
    return delegated_frame_host_.get();
  }

 protected:
  // RenderWidgetHostViewBase:
  void UpdateBackgroundColor() override;
  bool HasFallbackSurface() const override;

 private:
  friend class RenderWidgetHostViewAndroidTest;

  MouseWheelPhaseHandler* GetMouseWheelPhaseHandler() override;

  void EvictDelegatedFrame();

  bool ShouldRouteEvents() const;

  void UpdateTouchSelectionController(
      const viz::Selection<gfx::SelectionBound>& selection,
      float page_scale_factor,
      float top_controls_height,
      float top_controls_shown_ratio,
      const gfx::SizeF& scrollable_viewport_size_dip);
  bool UpdateControls(float dip_scale,
                      float top_controls_height,
                      float top_controls_shown_ratio,
                      float bottom_controls_height,
                      float bottom_controls_shown_ratio);
  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

  void OnFinishGetContentBitmap(const base::android::JavaRef<jobject>& obj,
                                const base::android::JavaRef<jobject>& callback,
                                const std::string& path,
                                const SkBitmap& bitmap);

  void ShowInternal();
  void HideInternal();
  void AttachLayers();
  void RemoveLayers();

  void EvictFrameIfNecessary();

  // Helper function to update background color for WebView on fullscreen
  // changes. See https://crbug.com/961223.
  void UpdateWebViewBackgroundColorIfNecessary();

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(
      const gfx::Rect& src_subrect_dip,
      const gfx::Size& dst_size_in_pixel,
      base::OnceCallback<void(const SkBitmap&)> callback);

  void EvictDelegatedContent();
  void OnLostResources();

  enum BeginFrameRequestType {
    BEGIN_FRAME = 1 << 0,
    PERSISTENT_BEGIN_FRAME = 1 << 1
  };
  void AddBeginFrameRequest(BeginFrameRequestType request);
  void ClearBeginFrameRequest(BeginFrameRequestType request);
  void StartObservingRootWindow();
  void StopObservingRootWindow();
  void SendBeginFramePaused();
  void SendBeginFrame(viz::BeginFrameArgs args);
  bool Animate(base::TimeTicks frame_time);
  void RequestDisallowInterceptTouchEvent();

  void ComputeEventLatencyOSTouchHistograms(const ui::MotionEvent& event);

  void CreateOverscrollControllerIfPossible();

  void UpdateMouseState(int action_button,
                        float mousedown_x,
                        float mouse_down_y);

  WebContentsAccessibilityAndroid* GetWebContentsAccessibilityAndroid() const;

  void OnFocusInternal();
  void LostFocusInternal();

  void SetTextHandlesHiddenForStylus(bool hide_handles);
  void SetTextHandlesHiddenInternal();

  void OnUpdateScopedSelectionHandles();

  // The begin frame source being observed.  Null if none.
  viz::BeginFrameSource* begin_frame_source_;
  viz::BeginFrameArgs last_begin_frame_args_;
  bool begin_frame_paused_ = false;
  bool wants_animate_only_begin_frames_ = false;

  // Indicates whether and for what reason a request for begin frames has been
  // issued. Used to control action dispatch at the next |OnBeginFrame()| call.
  uint32_t outstanding_begin_frame_requests_;

  bool is_showing_;

  // Window-specific bits that affect widget visibility.
  bool is_window_visible_;
  bool is_window_activity_started_;

  // Used to customize behavior for virtual reality mode, such as the
  // appearance of overscroll glow and the keyboard.
  bool is_in_vr_;

  // Specifies whether touch selection handles are hidden due to stylus.
  bool handles_hidden_by_stylus_ = false;

  // Specifies whether touch selection handles are hidden due to text selection.
  bool handles_hidden_by_selection_ui_ = false;

  ImeAdapterAndroid* ime_adapter_android_;
  SelectionPopupController* selection_popup_controller_;
  TextSuggestionHostAndroid* text_suggestion_host_;
  GestureListenerManager* gesture_listener_manager_;

  mutable ui::ViewAndroid view_;

  std::unique_ptr<ui::DelegatedFrameHostAndroid::Client>
      delegated_frame_host_client_;

  // Manages the Compositor Frames received from the renderer.
  std::unique_ptr<ui::DelegatedFrameHostAndroid> delegated_frame_host_;

  // The most recent surface size that was pushed to the surface layer.
  gfx::Size current_surface_size_;

  // Used to control and render overscroll-related effects.
  std::unique_ptr<OverscrollControllerAndroid> overscroll_controller_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  StylusTextSelector stylus_text_selector_;

  // Manages selection handle rendering and manipulation.
  // This will always be NULL if |content_view_core_| is NULL.
  std::unique_ptr<ui::TouchSelectionController> touch_selection_controller_;
  std::unique_ptr<ui::TouchSelectionControllerClient>
      touch_selection_controller_client_for_test_;
  // Keeps track of currently active touch selection controller clients (some
  // may be representing out-of-process iframes).
  std::unique_ptr<TouchSelectionControllerClientManagerAndroid>
      touch_selection_controller_client_manager_;
  // Notifies the WindowAndroid when the page has active selection handles.
  std::unique_ptr<ui::WindowAndroid::ScopedSelectionHandles>
      scoped_selection_handles_;

  // Bounds to use if we have no backing WebContents.
  gfx::Rect default_bounds_;

  const bool using_browser_compositor_;
  std::unique_ptr<SynchronousCompositorHost> sync_compositor_;

  SynchronousCompositorClient* synchronous_compositor_client_;

  bool observing_root_window_;

  bool controls_initialized_ = false;

  bool fallback_cursor_mode_enabled_;

  float prev_top_shown_pix_;
  float prev_top_controls_translate_;
  float prev_bottom_shown_pix_;
  float prev_bottom_controls_translate_;
  float page_scale_;
  float min_page_scale_;
  float max_page_scale_;

  base::TimeTicks prev_mousedown_timestamp_;
  gfx::Point prev_mousedown_point_;
  int left_click_count_ = 0;

  gfx::Insets insets_;

  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  base::ObserverList<DestructionObserver>::Unchecked destruction_observers_;

  MouseWheelPhaseHandler mouse_wheel_phase_handler_;
  uint32_t latest_capture_sequence_number_ = 0u;

  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  bool is_first_navigation_ = true;
  // If true, then the next allocated surface should be embedded.
  bool navigation_while_hidden_ = false;

  viz::FrameTimingDetailsMap timing_details_;

  // Tracks whether we are in SynchronousCopyContents to avoid repeated calls
  // into DevTools capture logic.
  // TODO(ericrk): Make this more robust.
  bool in_sync_copy_contents_ = false;

  // A cached copy of the most up to date DevToolsFrameMetadata, computed from
  // either RenderFrameMetadata or CompositorFrameMetadata.
  base::Optional<DevToolsFrameMetadata> last_devtools_frame_metadata_;

  WebContentsAccessibilityAndroid* web_contents_accessibility_ = nullptr;

  base::android::ScopedJavaGlobalRef<jobject> obj_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
