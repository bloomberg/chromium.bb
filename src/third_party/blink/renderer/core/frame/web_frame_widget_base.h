// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_BASE_H_

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/trees/layer_tree_host.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom-blink.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/page/drag.mojom-blink.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_battery_savings.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/page/page_widget_delegate.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace gfx {
class Point;
class PointF;
}  // namespace gfx

namespace blink {
class AnimationWorkletMutatorDispatcherImpl;
class FloatPoint;
class HitTestResult;
class HTMLPlugInElement;
class LocalFrameView;
class Page;
class PageWidgetEventHandler;
class PaintWorkletPaintDispatcher;
class RemoteFrame;
class WebLocalFrameImpl;
class WebPlugin;
class WebViewImpl;
class WidgetBase;
class ScreenMetricsEmulator;

class CORE_EXPORT WebFrameWidgetBase
    : public GarbageCollected<WebFrameWidgetBase>,
      public WebFrameWidget,
      public WidgetBaseClient,
      public mojom::blink::FrameWidget,
      public viz::mojom::blink::InputTargetClient,
      public FrameWidget,
      public PageWidgetEventHandler {
 public:
  WebFrameWidgetBase(
      WebWidgetClient&,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root);
  ~WebFrameWidgetBase() override;

  // Returns the WebFrame that this widget is attached to. It will be a local
  // root since only local roots have a widget attached.
  WebLocalFrameImpl* LocalRootImpl() const { return local_root_; }

  // Returns the bounding box of the block type node touched by the WebPoint.
  WebRect ComputeBlockBound(const gfx::Point& point_in_root_frame,
                            bool ignore_clipping) const;

  void BindLocalRoot(WebLocalFrame&);

  // If this widget is for the top level frame. This is different than
  // |ForMainFrame| because |ForMainFrame| could return true but this method
  // returns false. If this widget is a MainFrame widget embedded in another
  // widget, for example embedding a portal.
  virtual bool ForTopLevelFrame() const = 0;

  // Returns true if this widget is for a local root that is a child frame,
  // false otherwise.
  virtual bool ForSubframe() const = 0;

  // Opposite of |ForSubframe|. If this widget is for the local main frame.
  bool ForMainFrame() const { return !ForSubframe(); }

  virtual void IntrinsicSizingInfoChanged(
      mojom::blink::IntrinsicSizingInfoPtr) {}

  void AutoscrollStart(const gfx::PointF& position);
  void AutoscrollFling(const gfx::Vector2dF& position);
  void AutoscrollEnd();

  // Notifies RenderWidgetHostImpl that the frame widget has painted something.
  void DidMeaningfulLayout(WebMeaningfulLayout layout_type);

  bool HandleCurrentKeyboardEvent();

  // Creates or returns cached mutator dispatcher. This usually requires a
  // round trip to the compositor. The returned WeakPtr must only be
  // dereferenced on the output |mutator_task_runner|.
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
  EnsureCompositorMutatorDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner);

  // TODO: consider merge the input and return value to be one parameter.
  // Creates or returns cached paint dispatcher. The returned WeakPtr must only
  // be dereferenced on the output |paint_task_runner|.
  base::WeakPtr<PaintWorkletPaintDispatcher> EnsureCompositorPaintDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner>* paint_task_runner);

  HitTestResult CoreHitTestResultAt(const gfx::PointF&);

  // FrameWidget implementation.
  WebWidgetClient* Client() const final { return client_; }
  cc::AnimationHost* AnimationHost() const final;
  void SetOverscrollBehavior(
      const cc::OverscrollBehavior& overscroll_behavior) final;
  void RequestAnimationAfterDelay(const base::TimeDelta&) final;
  void RegisterSelection(cc::LayerSelection selection) final;
  void RequestDecode(const cc::PaintImage&,
                     base::OnceCallback<void(bool)>) final;
  void NotifySwapAndPresentationTimeInBlink(
      WebReportTimeCallback swap_callback,
      WebReportTimeCallback presentation_callback) final;
  void RequestBeginMainFrameNotExpected(bool request) final;
  int GetLayerTreeId() final;
  void SetEventListenerProperties(cc::EventListenerClass,
                                  cc::EventListenerProperties) final;
  cc::EventListenerProperties EventListenerProperties(
      cc::EventListenerClass) const final;
  mojom::blink::DisplayMode DisplayMode() const override;
  const WebVector<gfx::Rect>& WindowSegments() const override;
  void SetDelegatedInkMetadata(
      std::unique_ptr<viz::DelegatedInkMetadata> metadata) final;
  void DidOverscroll(const gfx::Vector2dF& overscroll_delta,
                     const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::PointF& position,
                     const gfx::Vector2dF& velocity) override;
  void InjectGestureScrollEvent(WebGestureDevice device,
                                const gfx::Vector2dF& delta,
                                ui::ScrollGranularity granularity,
                                cc::ElementId scrollable_area_element_id,
                                WebInputEvent::Type injected_type) override;
  void DidChangeCursor(const ui::Cursor&) override;
  void GetCompositionCharacterBoundsInWindow(
      Vector<gfx::Rect>* bounds_in_dips) override;
  gfx::Range CompositionRange() override;
  WebTextInputInfo TextInputInfo() override;
  ui::mojom::VirtualKeyboardVisibilityRequest
  GetLastVirtualKeyboardVisibilityRequest() override;
  bool ShouldSuppressKeyboardForFocusedElement() override;
  void GetEditContextBoundsInWindow(
      base::Optional<gfx::Rect>* control_bounds,
      base::Optional<gfx::Rect>* selection_bounds) override;
  int32_t ComputeWebTextInputNextPreviousFlags() override;
  void ResetVirtualKeyboardVisibilityRequest() override;
  bool GetSelectionBoundsInWindow(gfx::Rect* focus,
                                  gfx::Rect* anchor,
                                  base::i18n::TextDirection* focus_dir,
                                  base::i18n::TextDirection* anchor_dir,
                                  bool* is_anchor_first) override;
  void ClearTextInputState() override;

  bool SetComposition(const String& text,
                      const Vector<ui::ImeTextSpan>& ime_text_spans,
                      const gfx::Range& replacement_range,
                      int selection_start,
                      int selection_end) override;
  void CommitText(const String& text,
                  const Vector<ui::ImeTextSpan>& ime_text_spans,
                  const gfx::Range& replacement_range,
                  int relative_cursor_pos) override;
  void FinishComposingText(bool keep_selection) override;
  bool IsProvisional() override;
  uint64_t GetScrollableContainerIdAt(
      const gfx::PointF& point_in_dips) override;
  void SetEditCommandsForNextKeyEvent(
      Vector<mojom::blink::EditCommandPtr> edit_commands) override;

  void AddImeTextSpansToExistingText(
      uint32_t start,
      uint32_t end,
      const Vector<ui::ImeTextSpan>& ime_text_spans) override;
  Vector<ui::mojom::blink::ImeTextSpanInfoPtr> GetImeTextSpansInfo(
      const WebVector<ui::ImeTextSpan>& ime_text_spans) override;
  void ClearImeTextSpansByType(uint32_t start,
                               uint32_t end,
                               ui::ImeTextSpan::Type type) override;
  void SetCompositionFromExistingText(
      int32_t start,
      int32_t end,
      const Vector<ui::ImeTextSpan>& ime_text_spans) override;
  void ExtendSelectionAndDelete(int32_t before, int32_t after) override;
  void DeleteSurroundingText(int32_t before, int32_t after) override;
  void DeleteSurroundingTextInCodePoints(int32_t before,
                                         int32_t after) override;
  void SetEditableSelectionOffsets(int32_t start, int32_t end) override;
  void ExecuteEditCommand(const String& command, const String& value) override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void Delete() override;
  void SelectAll() override;
  void CollapseSelection() override;
  void Replace(const String& word) override;
  void ReplaceMisspelling(const String& word) override;
  void SelectRange(const gfx::Point& base_in_dips,
                   const gfx::Point& extent_in_dips) override;
  void AdjustSelectionByCharacterOffset(
      int32_t start,
      int32_t end,
      mojom::blink::SelectionMenuBehavior behavior) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent_in_dips) override;
  void ScrollFocusedEditableNodeIntoRect(
      const gfx::Rect& rect_in_dips) override;
  void MoveCaret(const gfx::Point& point_in_dips) override;
#if defined(OS_ANDROID)
  void SelectWordAroundCaret(SelectWordAroundCaretCallback callback) override;
#endif
  gfx::RectF BlinkSpaceToDIPs(const gfx::RectF& rect) override;
  gfx::Rect BlinkSpaceToEnclosedDIPs(const gfx::Rect& rect) override;
  gfx::Size BlinkSpaceToFlooredDIPs(const gfx::Size& size) override;
  gfx::RectF DIPsToBlinkSpace(const gfx::RectF& rect) override;
  gfx::PointF DIPsToBlinkSpace(const gfx::PointF& point) override;
  gfx::Point DIPsToRoundedBlinkSpace(const gfx::Point& point) override;
  float DIPsToBlinkSpace(float scalar) override;
  void RequestMouseLock(
      bool has_transient_user_activation,
      bool request_unadjusted_movement,
      mojom::blink::WidgetInputHandlerHost::RequestMouseLockCallback callback)
      override;
  bool CanComposeInline() override;
  bool ShouldDispatchImeEventsToPlugin() override;
  void ImeSetCompositionForPlugin(const String& text,
                                  const Vector<ui::ImeTextSpan>& ime_text_spans,
                                  const gfx::Range& replacement_range,
                                  int selection_start,
                                  int selection_end) override;
  void ImeCommitTextForPlugin(const String& text,
                              const Vector<ui::ImeTextSpan>& ime_text_spans,
                              const gfx::Range& replacement_range,
                              int relative_cursor_pos) override;
  void ImeFinishComposingTextForPlugin(bool keep_selection) override;

  // WebFrameWidget implementation.
  WebLocalFrame* LocalRoot() const override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  WebInputMethodController* GetActiveWebInputMethodController() const override;
  WebLocalFrame* FocusedWebLocalFrameInWidget() const override;
  void ApplyViewportChangesForTesting(
      const ApplyViewportChangesArgs& args) override;
  void NotifySwapAndPresentationTime(
      WebReportTimeCallback swap_callback,
      WebReportTimeCallback presentation_callback) override;
  scheduler::WebRenderWidgetSchedulingState* RendererWidgetSchedulingState()
      override;
  void WaitForDebuggerWhenShown() override;
  void SetTextZoomFactor(float text_zoom_factor) override;
  float TextZoomFactor() override;
  void SetMainFrameOverlayColor(SkColor) override;
  void AddEditCommandForNextKeyEvent(const WebString& name,
                                     const WebString& value) override;
  void ClearEditCommands() override;
  bool IsPasting() override;
  bool HandlingSelectRange() override;
  void ReleaseMouseLockAndPointerCaptureForTesting() override;
  const viz::FrameSinkId& GetFrameSinkId() override;
  WebHitTestResult HitTestResultAt(const gfx::PointF&) override;

  // Called when a drag-n-drop operation should begin.
  void StartDragging(const WebDragData&,
                     DragOperationsMask,
                     const SkBitmap& drag_image,
                     const gfx::Point& drag_image_offset);

  bool DoingDragAndDrop() { return doing_drag_and_drop_; }
  static void SetIgnoreInputEvents(bool value) { ignore_input_events_ = value; }
  static bool IgnoreInputEvents() { return ignore_input_events_; }

  // WebWidget methods.
  cc::LayerTreeHost* InitializeCompositing(
      scheduler::WebThreadScheduler* main_thread_scheduler,
      cc::TaskGraphRunner* task_graph_runner,
      bool for_child_local_root_frame,
      const ScreenInfo& screen_info,
      std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory,
      const cc::LayerTreeSettings* settings) override;
  void Close(
      scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) override;
  void SetCompositorVisible(bool visible) override;
  void SetCursor(const ui::Cursor& cursor) override;
  bool HandlingInputEvent() override;
  void SetHandlingInputEvent(bool handling) override;
  void ProcessInputEventSynchronouslyForTesting(const WebCoalescedInputEvent&,
                                                HandledEventCallback) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void UpdateTextInputState() override;
  void UpdateSelectionBounds() override;
  void ShowVirtualKeyboard() override;
  bool HasFocus() override;
  void SetFocus(bool focus) override;
  void FlushInputProcessedCallback() override;
  void CancelCompositionForPepper() override;
  void ApplyVisualProperties(
      const VisualProperties& visual_properties) override;
  bool PinchGestureActiveInMainFrame() override;
  float PageScaleInMainFrame() override;
  const ScreenInfo& GetScreenInfo() override;
  gfx::Rect WindowRect() override;
  gfx::Rect ViewRect() override;
  void SetScreenRects(const gfx::Rect& widget_screen_rect,
                      const gfx::Rect& window_screen_rect) override;
  gfx::Size VisibleViewportSizeInDIPs() override;
  bool IsHidden() const override;
  WebString GetLastToolTipTextForTesting() const override;

  // WidgetBaseClient methods.
  void BeginMainFrame(base::TimeTicks last_frame_time) override;
  void BeginCommitCompositorFrame() override;
  void EndCommitCompositorFrame(base::TimeTicks commit_start_time) override;
  void RecordDispatchRafAlignedInputTime(
      base::TimeTicks raf_aligned_input_start_time) override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(
      base::TimeTicks,
      cc::ActiveFrameSequenceTrackers trackers) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void DidCommitAndDrawCompositorFrame() override;
  std::unique_ptr<cc::LayerTreeFrameSink> AllocateNewLayerTreeFrameSink()
      override;
  void DidObserveFirstScrollDelay(
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;
  void DidBeginMainFrame() override;
  void WillBeginMainFrame() override;
  void DidCompletePageScaleAnimation() override;
  void FocusChangeComplete() override;
  bool WillHandleGestureEvent(const WebGestureEvent& event) override;
  void WillHandleMouseEvent(const WebMouseEvent& event) override;
  void ObserveGestureEventAndResult(
      const WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      const cc::OverscrollBehavior& overscroll_behavior,
      bool event_processed) override;
  bool SupportsBufferedTouchEvents() override { return true; }
  void DidHandleKeyEvent() override;
  WebTextInputType GetTextInputType() override;
  void SetCursorVisibilityState(bool is_visible) override;
  blink::FrameWidget* FrameWidget() override { return this; }
  void ScheduleAnimation() override;
  bool ShouldAckSyntheticInputImmediately() override;
  void UpdateVisualProperties(
      const VisualProperties& visual_properties) override;
  void ScheduleAnimationForWebTests() override;
  void OrientationChanged() override;
  void DidUpdateSurfaceAndScreen(
      const ScreenInfo& previous_original_screen_info) override;
  const ScreenInfo& GetOriginalScreenInfo() override;
  base::Optional<blink::mojom::ScreenOrientation> ScreenOrientationOverride()
      override;
  void WasHidden() override;
  void WasShown(bool was_evicted) override;
  KURL GetURLForDebugTrace() override;

  // mojom::blink::FrameWidget methods.
  void DragTargetDragEnter(const WebDragData&,
                           const gfx::PointF& point_in_viewport,
                           const gfx::PointF& screen_point,
                           DragOperationsMask operations_allowed,
                           uint32_t key_modifiers,
                           DragTargetDragEnterCallback callback) override;
  void DragTargetDragOver(const gfx::PointF& point_in_viewport,
                          const gfx::PointF& screen_point,
                          DragOperationsMask operations_allowed,
                          uint32_t key_modifiers,
                          DragTargetDragOverCallback callback) override;
  void DragTargetDragLeave(const gfx::PointF& point_in_viewport,
                           const gfx::PointF& screen_point) override;
  void DragTargetDrop(const WebDragData&,
                      const gfx::PointF& point_in_viewport,
                      const gfx::PointF& screen_point,
                      uint32_t key_modifiers) override;
  void DragSourceEndedAt(const gfx::PointF& point_in_viewport,
                         const gfx::PointF& screen_point,
                         DragOperation) override;
  void DragSourceSystemDragEnded() override;
  void SetBackgroundOpaque(bool opaque) override;
  void SetActive(bool active) override;
  // For both mainframe and childframe change the text direction of the
  // currently selected input field (if any).
  void SetTextDirection(base::i18n::TextDirection direction) override;
  // Sets the inherited effective touch action on an out-of-process iframe.
  void SetInheritedEffectiveTouchActionForSubFrame(
      WebTouchAction touch_action) override {}
  // Toggles render throttling for an out-of-process iframe. Local frames are
  // throttled based on their visibility in the viewport, but remote frames
  // have to have throttling information propagated from parent to child
  // across processes.
  void UpdateRenderThrottlingStatusForSubFrame(
      bool is_throttled,
      bool subtree_throttled) override {}
  void ShowContextMenu(ui::mojom::MenuSourceType source_type,
                       const gfx::Point& location) override;
  void SetViewportIntersection(
      mojom::blink::ViewportIntersectionStatePtr intersection_state) override {}

  // Sets the inert bit on an out-of-process iframe, causing it to ignore
  // input.
  void SetIsInertForSubFrame(bool inert) override {}
#if defined(OS_MAC)
  void GetStringAtPoint(const gfx::Point& point_in_local_root,
                        GetStringAtPointCallback callback) override;
#endif

  // Sets the display mode, which comes from the top-level browsing context and
  // is applied to all widgets.
  void SetDisplayMode(mojom::blink::DisplayMode);

  base::Optional<gfx::Point> GetAndResetContextMenuLocation();

  void BindWidgetCompositor(
      mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) override;

  void BindInputTargetClient(
      mojo::PendingReceiver<viz::mojom::blink::InputTargetClient> receiver)
      override;

  // viz::mojom::blink::InputTargetClient:
  void FrameSinkIdAt(const gfx::PointF& point,
                     const uint64_t trace_id,
                     FrameSinkIdAtCallback callback) override;

  // Called when the FrameView for this Widget's local root is created.
  virtual void DidCreateLocalRootView() {}

  virtual void SetZoomLevel(double zoom_level);

  // Enable or disable auto-resize. This is part of
  // UpdateVisualProperties though tests may call to it more directly.
  virtual void SetAutoResizeMode(bool auto_resize,
                                 const gfx::Size& min_size_before_dsf,
                                 const gfx::Size& max_size_before_dsf,
                                 float device_scale_factor) = 0;

  // This method returns the focused frame belonging to this WebWidget, that
  // is, a focused frame with the same local root as the one corresponding
  // to this widget. It will return nullptr if no frame is focused or, the
  // focused frame has a different local root.
  LocalFrame* FocusedLocalFrameInWidget() const;

  virtual void Trace(Visitor*) const;

  // For when the embedder itself change scales on the page (e.g. devtools)
  // and wants all of the content at the new scale to be crisp
  void SetNeedsRecalculateRasterScales();

  // Sets the background color to be filled in as gutter behind/around the
  // painted content. Non-composited WebViews need not implement this, as they
  // paint into another widget which has a background color of its own.
  void SetBackgroundColor(SkColor color);

  // Starts an animation of the page scale to a target scale factor and scroll
  // offset.
  // If use_anchor is true, destination is a point on the screen that will
  // remain fixed for the duration of the animation.
  // If use_anchor is false, destination is the final top-left scroll position.
  void StartPageScaleAnimation(const gfx::Vector2d& destination,
                               bool use_anchor,
                               float new_page_scale,
                               base::TimeDelta duration);

  // Called to update if scroll events should be sent.
  void SetHaveScrollEventHandlers(bool);

  // Start deferring commits to the compositor, allowing document lifecycle
  // updates without committing the layer tree. Commits are deferred
  // until at most the given |timeout| has passed. If multiple calls are made
  // when deferral is active then the initial timeout applies.
  void StartDeferringCommits(base::TimeDelta timeout);
  // Immediately stop deferring commits.
  void StopDeferringCommits(cc::PaintHoldingCommitTrigger);

  // Prevents any updates to the input for the layer tree, and the layer tree
  // itself, and the layer tree from becoming visible.
  std::unique_ptr<cc::ScopedDeferMainFrameUpdate> DeferMainFrameUpdate();

  // Sets the amount that the top and bottom browser controls are showing, from
  // 0 (hidden) to 1 (fully shown).
  void SetBrowserControlsShownRatio(float top_ratio, float bottom_ratio);

  // Set browser controls params. These params consist of top and bottom
  // heights, min-heights, browser_controls_shrink_blink_size, and
  // animate_browser_controls_height_changes. If
  // animate_browser_controls_height_changes is set to true, changes to the
  // browser controls height will be animated. If
  // browser_controls_shrink_blink_size is set to true, then Blink shrunk the
  // viewport clip layers by the top and bottom browser controls height. Top
  // controls will translate the web page down and do not immediately scroll
  // when hiding. The bottom controls scroll immediately and never translate the
  // content (only clip it).
  void SetBrowserControlsParams(cc::BrowserControlsParams params);

  cc::LayerTreeDebugState GetLayerTreeDebugState();
  void SetLayerTreeDebugState(const cc::LayerTreeDebugState& state);

  // Ask compositor to composite a frame for testing. This will generate a
  // BeginMainFrame, and update the document lifecycle.
  void SynchronouslyCompositeForTesting(base::TimeTicks frame_time);

  void SetToolTipText(const String& tooltip_text, TextDirection dir);

  void ShowVirtualKeyboardOnElementFocus();
  void ProcessTouchAction(WebTouchAction touch_action);

  // Called when a gesture event has been processed.
  void DidHandleGestureEvent(const WebGestureEvent& event,
                             bool event_cancelled);

  // Called to update if pointerrawupdate events should be sent.
  void SetHasPointerRawUpdateEventHandlers(bool);

  // Called to update whether low latency input mode is enabled or not.
  void SetNeedsLowLatencyInput(bool);

  // Requests unbuffered (ie. low latency) input until a pointerup
  // event occurs.
  void RequestUnbufferedInputEvents();

  // Requests unbuffered (ie. low latency) input due to debugger being
  // attached. Debugger needs to paint when stopped in the event handler.
  void SetNeedsUnbufferedInputForDebugger(bool);

  // Called when the main frame navigates.
  void DidNavigate();

  // Called when the widget should get targeting input.
  void SetMouseCapture(bool capture);

  // Sets the current page scale factor and minimum / maximum limits. Both
  // limits are initially 1 (no page scale allowed).
  virtual void SetPageScaleStateAndLimits(float page_scale_factor,
                                          bool is_pinch_gesture_active,
                                          float minimum,
                                          float maximum);

  // The value of the applied battery-savings META element in the document
  // changed.
  void BatterySavingsChanged(WebBatterySavingsFlags savings);

  const viz::LocalSurfaceId& LocalSurfaceIdFromParent();
  cc::LayerTreeHost* LayerTreeHost();

  virtual ScreenMetricsEmulator* DeviceEmulator() { return nullptr; }

  // Called during |UpdateVisualProperties| to apply the new size to the widget.
  virtual void ApplyVisualPropertiesSizing(
      const VisualProperties& visual_properties) = 0;

  // Calculates the selection bounds in the root frame. Returns bounds unchanged
  // when there is no focused frame or no selection.
  virtual void CalculateSelectionBounds(gfx::Rect& anchor_in_root_frame,
                                        gfx::Rect& focus_in_root_frame) = 0;

  // Update the surface allocation information, compositor viewport rect and
  // screen info on the widget.
  void UpdateSurfaceAndScreenInfo(
      const viz::LocalSurfaceId& new_local_surface_id,
      const gfx::Rect& compositor_viewport_pixel_rect,
      const ScreenInfo& new_screen_info);
  // Similar to UpdateSurfaceAndScreenInfo but the surface allocation
  // and compositor viewport rect remains the same.
  void UpdateScreenInfo(const ScreenInfo& screen_info);
  void UpdateSurfaceAndCompositorRect(
      const viz::LocalSurfaceId& new_local_surface_id,
      const gfx::Rect& compositor_viewport_pixel_rect);
  void UpdateCompositorViewportRect(
      const gfx::Rect& compositor_viewport_pixel_rect);
  void SetWindowSegments(const std::vector<gfx::Rect>& window_segments);
  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  // Set the pending window rect. For every SetPendingWindowRect
  // call there must be an AckPendingWindowRect call.
  void SetPendingWindowRect(const gfx::Rect& window_screen_rect);

  // Clear a previously set pending window rect. For every SetPendingWindowRect
  // call there must be an AckPendingWindowRect call.
  void AckPendingWindowRect();

  // Constrains the viewport intersection for use by IntersectionObserver,
  // and indicates whether the frame may be painted over or obscured in the
  // parent. This is needed for out-of-process iframes to know if they are
  // clipped or obscured by ancestor frames in another process.
  virtual void SetRemoteViewportIntersection(
      const mojom::blink::ViewportIntersectionState& intersection_state) {}

  // Return the focused WebPlugin if there is one.
  WebPlugin* GetFocusedPluginContainer();

 protected:
  enum DragAction { kDragEnter, kDragOver };

  // Consolidate some common code between starting a drag over a target and
  // updating a drag over a target. If we're starting a drag, |isEntering|
  // should be true.
  DragOperation DragTargetDragEnterOrOver(const gfx::PointF& point_in_viewport,
                                          const gfx::PointF& screen_point,
                                          DragAction,
                                          uint32_t key_modifiers);

  // Helper function to call VisualViewport::viewportToRootFrame().
  gfx::PointF ViewportToRootFrame(const gfx::PointF& point_in_viewport) const;

  WebViewImpl* View() const;

  // Returns the page object associated with this widget. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* GetPage() const;

  mojom::blink::FrameWidgetHost* GetAssociatedFrameWidgetHost() const;

  // Helper function to process events while pointer locked.
  void PointerLockMouseEvent(const WebCoalescedInputEvent&);
  bool IsPointerLocked();

  // The fullscreen granted status from the most recent VisualProperties update.
  bool IsFullscreenGranted();

  // Return the LocalFrameView used for animation scrolling. This is overridden
  // by WebViewFrameWidget and should eventually be removed once null does not
  // need to be passed for the main frame.
  virtual LocalFrameView* GetLocalFrameViewForAnimationScrolling() = 0;

  void NotifyPageScaleFactorChanged(float page_scale_factor,
                                    bool is_pinch_gesture_active);

  // Helper for notifying frame-level objects that care about input events.
  // TODO: With some effort, this could be folded into a common implementation
  // of WebViewImpl::HandleInputEvent and WebFrameWidgetImpl::HandleInputEvent.
  void NotifyInputObservers(const WebCoalescedInputEvent& coalesced_event);

  Frame* FocusedCoreFrame() const;

  // Perform a hit test for a point relative to the root frame of the page.
  HitTestResult HitTestResultForRootFramePos(
      const FloatPoint& pos_in_root_frame);

  // A copy of the web drop data object we received from the browser.
  Member<DataObject> current_drag_data_;

  bool doing_drag_and_drop_ = false;

  // The available drag operations (copy, move link...) allowed by the source.
  DragOperation operations_allowed_ = kDragOperationNone;

  // The current drag operation as negotiated by the source and destination.
  // When not equal to DragOperationNone, the drag data can be dropped onto the
  // current drop target in this WebView (the drop target can accept the drop).
  DragOperation drag_operation_ = kDragOperationNone;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  mojom::blink::DragEventSourceInfo possible_drag_event_info_;

  // Base functionality all widgets have. This is a member as to avoid
  // complicated inheritance structures.
  std::unique_ptr<WidgetBase> widget_base_;

  // The last seen page scale state, which comes from the main frame if we're
  // in a child frame. This state is propagated through the RenderWidget tree
  // passed to any new child RenderWidget.
  float page_scale_factor_in_mainframe_ = 1.f;
  bool is_pinch_gesture_active_in_mainframe_ = false;

  // If set, the (plugin) element which has mouse capture.
  // TODO(dtapuska): Move to private once all input handling is moved to
  // base class.
  Member<HTMLPlugInElement> mouse_capture_element_;

  // keyPress events to be suppressed if the associated keyDown event was
  // handled.
  // TODO(dtapuska): Move to private once all input handling is moved to
  // base class.
  bool suppress_next_keypress_event_ = false;

 private:
  // PageWidgetEventHandler methods:
  void HandleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;

  WebInputEventResult HandleCapturedMouseEvent(const WebCoalescedInputEvent&);
  void MouseContextMenu(const WebMouseEvent&);
  void CancelDrag();
  void RequestAnimationAfterDelayTimerFired(TimerBase*);
  void PresentationCallbackForMeaningfulLayout(blink::WebSwapResult,
                                               base::TimeTicks);

  void ForEachRemoteFrameControlledByWidget(
      const base::RepeatingCallback<void(RemoteFrame*)>& callback);

  static bool ignore_input_events_;

  WebWidgetClient* client_;

  const viz::FrameSinkId frame_sink_id_;

  // WebFrameWidget is associated with a subtree of the frame tree,
  // corresponding to a maximal connected tree of LocalFrames. This member
  // points to the root of that subtree.
  Member<WebLocalFrameImpl> local_root_;

  mojom::blink::DisplayMode display_mode_;

  WebVector<gfx::Rect> window_segments_;

  // This is owned by the LayerTreeHostImpl, and should only be used on the
  // compositor thread, so we keep the TaskRunner where you post tasks to
  // make that happen.
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl> mutator_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> mutator_task_runner_;

  // The |paint_dispatcher_| should only be dereferenced on the
  // |paint_task_runner_| (in practice this is the compositor thread). We keep a
  // copy of it here to provide to new PaintWorkletProxyClient objects (which
  // run on the worklet thread) so that they can talk to the
  // PaintWorkletPaintDispatcher on the compositor thread.
  base::WeakPtr<PaintWorkletPaintDispatcher> paint_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> paint_task_runner_;

  std::unique_ptr<TaskRunnerTimer<WebFrameWidgetBase>>
      request_animation_after_delay_timer_;

  // WebFrameWidgetBase is not tied to ExecutionContext
  HeapMojoAssociatedRemote<mojom::blink::FrameWidgetHost,
                           HeapMojoWrapperMode::kWithoutContextObserver>
      frame_widget_host_{nullptr};
  // WebFrameWidgetBase is not tied to ExecutionContext
  HeapMojoAssociatedReceiver<mojom::blink::FrameWidget,
                             WebFrameWidgetBase,
                             HeapMojoWrapperMode::kWithoutContextObserver>
      receiver_{this, nullptr};
  HeapMojoReceiver<viz::mojom::blink::InputTargetClient,
                   WebFrameWidgetBase,
                   HeapMojoWrapperMode::kWithoutContextObserver>
      input_target_receiver_{this, nullptr};

  // Different consumers in the browser process makes different assumptions, so
  // must always send the first IPC regardless of value.
  base::Optional<bool> has_touch_handlers_;

  Vector<mojom::blink::EditCommandPtr> edit_commands_;

  base::Optional<gfx::Point> host_context_menu_location_;
  uint32_t last_capture_sequence_number_ = 0u;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_ = false;

  // Indicates whether we need to consume scroll gestures to move cursor.
  bool swipe_to_move_cursor_activated_ = false;

  // Set when a measurement begins, reset when the measurement is taken.
  base::Optional<base::TimeTicks> update_layers_start_time_;

  // Metrics for gathering time for commit of compositor frame.
  base::Optional<base::TimeTicks> commit_compositor_frame_start_time_;

  friend class WebViewImpl;
  friend class ReportTimeSwapPromise;
};

template <>
struct DowncastTraits<WebFrameWidgetBase> {
  // All concrete implementations of WebFrameWidget are derived from
  // WebFrameWidgetBase.
  static bool AllowFrom(const WebFrameWidget& widget) { return true; }
};

}  // namespace blink

#endif
