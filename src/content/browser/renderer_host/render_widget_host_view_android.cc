// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include <limits>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/android/overscroll_controller_android.h"
#include "content/browser/android/selection/selection_popup_controller.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/android/text_suggestion_host_android.h"
#include "content/browser/bad_message.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/delegated_frame_host_client_android.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/common/content_switches_internal.h"
#include "content/public/android/content_jni_headers/RenderWidgetHostViewImpl_jni.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display_util.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

namespace {

static const char kAsyncReadBackString[] = "Compositing.CopyFromSurfaceTime";
static const base::TimeDelta kClickCountInterval = base::Seconds(0.5);
static const float kClickCountRadiusSquaredDIP = 25;

std::unique_ptr<ui::TouchSelectionController> CreateSelectionController(
    ui::TouchSelectionControllerClient* client,
    bool has_view_tree) {
  DCHECK(client);
  DCHECK(has_view_tree);
  ui::TouchSelectionController::Config config;
  config.max_tap_duration =
      base::Milliseconds(gfx::ViewConfiguration::GetLongPressTimeoutInMs());
  config.tap_slop = gfx::ViewConfiguration::GetTouchSlopInDips();
  config.enable_adaptive_handle_orientation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAdaptiveSelectionHandleOrientation);
  config.enable_longpress_drag_selection =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLongpressDragSelection);
  config.hide_active_handle =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P;
  return std::make_unique<ui::TouchSelectionController>(client, config);
}

gfx::RectF GetSelectionRect(const ui::TouchSelectionController& controller) {
  // When the touch handles are on the same line, the rect may become simply a
  // one-dimensional rect, and still need to union the handle rect to avoid the
  // context menu covering the touch handle. See detailed comments in
  // TouchSelectionController::GetRectBetweenBounds(). Ensure that the |rect| is
  // not empty by adding a pixel width or height to avoid the wrong menu
  // position.
  gfx::RectF rect = controller.GetVisibleRectBetweenBounds();
  if (rect.IsEmpty()) {
    gfx::SizeF size = rect.size();
    size.SetToMax(gfx::SizeF(1.0f, 1.0f));
    rect.set_size(size);
  }

  rect.Union(controller.GetStartHandleRect());
  rect.Union(controller.GetEndHandleRect());
  return rect;
}

void RecordToolTypeForActionDown(const ui::MotionEventAndroid& event) {
  ui::MotionEventAndroid::Action action = event.GetAction();
  if (action == ui::MotionEventAndroid::Action::DOWN ||
      action == ui::MotionEventAndroid::Action::POINTER_DOWN ||
      action == ui::MotionEventAndroid::Action::BUTTON_PRESS) {
    UMA_HISTOGRAM_ENUMERATION(
        "Event.AndroidActionDown.ToolType",
        static_cast<int>(event.GetToolType(0)),
        static_cast<int>(ui::MotionEventAndroid::ToolType::LAST) + 1);
  }
}

void WakeUpGpu(GpuProcessHost* host) {
  if (host && host->gpu_host()->wake_up_gpu_before_drawing()) {
    host->gpu_service()->WakeUpGpu();
  }
}

std::string CompressAndSaveBitmap(const std::string& dir,
                                  const SkBitmap& bitmap) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(bitmap, 85, &data)) {
    LOG(ERROR) << "Failed to encode bitmap to JPEG";
    return std::string();
  }

  base::FilePath screenshot_dir(dir);
  if (!base::DirectoryExists(screenshot_dir)) {
    if (!base::CreateDirectory(screenshot_dir)) {
      LOG(ERROR) << "Failed to create screenshot directory";
      return std::string();
    }
  }

  base::FilePath screenshot_path;
  base::ScopedFILE out_file(base::CreateAndOpenTemporaryStreamInDir(
      screenshot_dir, &screenshot_path));
  if (!out_file) {
    LOG(ERROR) << "Failed to create temporary screenshot file";
    return std::string();
  }
  unsigned int bytes_written =
      fwrite(reinterpret_cast<const char*>(data.data()), 1, data.size(),
             out_file.get());
  out_file.reset();  // Explicitly close before a possible Delete below.

  // If there were errors, don't leave a partial file around.
  if (bytes_written != data.size()) {
    base::DeleteFile(screenshot_path);
    LOG(ERROR) << "Error writing screenshot file to disk";
    return std::string();
  }
  return screenshot_path.value();
}

}  // namespace

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    gfx::NativeView parent_native_view)
    : RenderWidgetHostViewBase(widget_host),
      is_showing_(!widget_host->is_hidden()),
      is_window_visible_(true),
      is_window_activity_started_(true),
      is_in_vr_(false),
      ime_adapter_android_(nullptr),
      selection_popup_controller_(nullptr),
      text_suggestion_host_(nullptr),
      gesture_listener_manager_(nullptr),
      view_(ui::ViewAndroid::LayoutType::MATCH_PARENT),
      gesture_provider_(
          ui::GetGestureProviderConfig(
              ui::GestureProviderConfigType::CURRENT_PLATFORM,
              content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})),
          this),
      stylus_text_selector_(this),
      using_browser_compositor_(CompositorImpl::IsInitialized()),
      synchronous_compositor_client_(nullptr),
      observing_root_window_(false),
      prev_top_shown_pix_(0.f),
      prev_top_controls_pix_(0.f),
      prev_top_controls_translate_(0.f),
      prev_top_controls_min_height_offset_pix_(0.f),
      prev_bottom_shown_pix_(0.f),
      prev_bottom_controls_translate_(0.f),
      prev_bottom_controls_min_height_offset_pix_(0.f),
      page_scale_(1.f),
      min_page_scale_(1.f),
      max_page_scale_(1.f),
      mouse_wheel_phase_handler_(this),
      is_surface_sync_throttling_(features::IsSurfaceSyncThrottling()) {
  // Set the layer which will hold the content layer for this view. The content
  // layer is managed by the DelegatedFrameHost.
  view_.SetLayer(cc::Layer::Create());
  view_.set_event_handler(this);

  // If we're showing at creation time, we won't get a visibility change, so
  // generate our initial LocalSurfaceId here.
  if (is_showing_)
    local_surface_id_allocator_.GenerateId();

  delegated_frame_host_client_ =
      std::make_unique<DelegatedFrameHostClientAndroid>(this);
  delegated_frame_host_ = std::make_unique<ui::DelegatedFrameHostAndroid>(
      &view_, GetHostFrameSinkManager(), delegated_frame_host_client_.get(),
      host()->GetFrameSinkId());
  if (is_showing_) {
    delegated_frame_host_->WasShown(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), host()->delegate()->IsFullscreen());
  }

  // Let the page-level input event router know about our frame sink ID
  // for surface-based hit testing.
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

  host()->SetView(this);
  touch_selection_controller_client_manager_ =
      std::make_unique<TouchSelectionControllerClientManagerAndroid>(
          this, GetHostFrameSinkManager());

  UpdateNativeViewTree(parent_native_view);
  // This RWHVA may have been created speculatively. We should give any
  // existing RWHVAs priority for receiving input events, otherwise a
  // speculative RWHVA could be sent input events intended for the currently
  // showing RWHVA.
  if (parent_native_view)
    parent_native_view->MoveToBack(&view_);

  CreateOverscrollControllerIfPossible();

  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);

  host()->render_frame_metadata_provider()->AddObserver(this);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  UpdateNativeViewTree(nullptr);
  view_.set_event_handler(nullptr);
  DCHECK(!ime_adapter_android_);
  DCHECK(!delegated_frame_host_);
  if (obj_) {
    Java_RenderWidgetHostViewImpl_clearNativePtr(
        base::android::AttachCurrentThread(), obj_);
    obj_.Reset();
  }
}

void RenderWidgetHostViewAndroid::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void RenderWidgetHostViewAndroid::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

base::CallbackListSubscription
RenderWidgetHostViewAndroid::SubscribeToSurfaceIdChanges(
    const SurfaceIdChangedCallback& callback) {
  return surface_id_changed_callbacks_.Add(callback);
}

void RenderWidgetHostViewAndroid::OnSurfaceIdChanged() {
  surface_id_changed_callbacks_.Notify(GetCurrentSurfaceId());
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos,
    const gfx::Rect& anchor_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  RenderFrameHostImpl* frame_host = host()->frame_tree()->GetMainFrame();
  if (!frame_host || !frame_host->GetPage().virtual_keyboard_overlays_content())
    return;
  gfx::Rect keyboard_rect_with_scale;
  if (!keyboard_rect.IsEmpty()) {
    float scale = IsUseZoomForDSFEnabled() ? 1 / view_.GetDipScale() : 1.f;
    keyboard_rect_with_scale = ScaleToEnclosedRect(keyboard_rect, scale);
    // Intersect the keyboard rect with the `this` bounds which will be sent
    // to the renderer.
    keyboard_rect_with_scale.Intersect(GetViewBounds());
  }
  frame_host->NotifyVirtualKeyboardOverlayRect(keyboard_rect_with_scale);
}

bool RenderWidgetHostViewAndroid::ShouldVirtualKeyboardOverlayContent() {
  RenderFrameHostImpl* frame_host = host()->frame_tree()->GetMainFrame();
  return frame_host &&
         frame_host->GetPage().virtual_keyboard_overlays_content();
}

bool RenderWidgetHostViewAndroid::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const absl::optional<viz::LocalSurfaceId>& child_local_surface_id) {
  if (!CanSynchronizeVisualProperties())
    return false;
  if (child_local_surface_id) {
    local_surface_id_allocator_.UpdateFromChild(*child_local_surface_id);
  } else {
    local_surface_id_allocator_.GenerateId();
    // When a rotation begins while hidden, the Renderer will report the amount
    // of time spent performing layout of the incremental surfaces. We cache the
    // first viz::LocalSurfaceId sent, and then update |hidden_rotation_time_|
    // for all subsequent cc::RenderFrameMetadata reported until the rotation
    // completes.
    if (!is_showing_ && in_rotation_ &&
        !first_hidden_local_surface_id_.is_valid()) {
      hidden_rotation_time_ = base::TimeDelta();
      first_hidden_local_surface_id_ =
          local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    }
  }

  // If we still have an invalid viz::LocalSurfaceId, then we are hidden and
  // evicted. This will have been triggered by a child acknowledging a previous
  // synchronization message via DidUpdateVisualProperties. The child has not
  // prompted any further property changes, so we do not need to continue
  // syncrhonization. Nor do we want to embed an invalid surface.
  if (!local_surface_id_allocator_.HasValidLocalSurfaceId())
    return false;

  if (delegated_frame_host_) {
    delegated_frame_host_->EmbedSurface(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), deadline_policy,
        host()->delegate()->IsFullscreen());
  }

  return host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_bounds_ = gfx::Rect(default_bounds_.origin(), size);
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  default_bounds_ = rect;
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!delegated_frame_host_)
    return false;

  if (!view_.parent())
    return false;

  if (current_surface_size_.IsEmpty())
    return false;

  return delegated_frame_host_->HasSavedFrame();
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() {
  return &view_;
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::GotFocus() {
  host()->GotFocus();
  OnFocusInternal();
}

void RenderWidgetHostViewAndroid::LostFocus() {
  host()->LostFocus();
  LostFocusInternal();
}

void RenderWidgetHostViewAndroid::OnRenderFrameMetadataChangedBeforeActivation(
    const cc::RenderFrameMetadata& metadata) {
  // If we began Surface Synchronization while hidden, the Renderer will report
  // the time spent performing the incremental layouts. The record those here,
  // to be included with the final time spend completing rotation in
  // OnRenderFrameMetadataChangedAfterActivation.
  if (first_hidden_local_surface_id_.is_valid() &&
      metadata.local_surface_id->is_valid()) {
    auto local_surface_id = metadata.local_surface_id.value();
    // We stop recording layout times once the surface is expected as the final
    // one for rotation. For that surface we are interested in the full time
    // until activation. Which will include layout and rendering.
    if (!rotation_metrics_.empty() &&
        local_surface_id.IsSameOrNewerThan(rotation_metrics_.front().second)) {
      first_hidden_local_surface_id_ = viz::LocalSurfaceId();
    } else if (metadata.local_surface_id->IsSameOrNewerThan(
                   first_hidden_local_surface_id_)) {
      hidden_rotation_time_ += metadata.visual_properties_update_duration;
    }
  }

  bool is_transparent = metadata.has_transparent_background;
  SkColor root_background_color = metadata.root_background_color;

  if (!using_browser_compositor_) {
    // DevTools ScreenCast support for Android WebView.
    last_render_frame_metadata_ = metadata;
    // Android WebView ignores transparent background.
    is_transparent = false;
  }

  gesture_provider_.SetDoubleTapSupportForPageEnabled(
      !metadata.is_mobile_optimized);

  float dip_scale = view_.GetDipScale();
  gfx::SizeF root_layer_size_dip = metadata.root_layer_size;
  gfx::SizeF scrollable_viewport_size_dip = metadata.scrollable_viewport_size;
  gfx::PointF root_scroll_offset_dip =
      metadata.root_scroll_offset.value_or(gfx::PointF());
  if (IsUseZoomForDSFEnabled()) {
    float pix_to_dip = 1 / dip_scale;
    root_layer_size_dip.Scale(pix_to_dip);
    scrollable_viewport_size_dip.Scale(pix_to_dip);
    root_scroll_offset_dip.Scale(pix_to_dip);
  }

  float to_pix = IsUseZoomForDSFEnabled() ? 1.f : dip_scale;
  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset =
      metadata.top_controls_height * metadata.top_controls_shown_ratio;
  float top_shown_pix = top_content_offset * to_pix;

  if (ime_adapter_android_) {
    ime_adapter_android_->UpdateFrameInfo(metadata.selection.start, dip_scale,
                                          top_shown_pix);
  }

  if (!gesture_listener_manager_)
    return;

  UpdateTouchSelectionController(metadata.selection, metadata.page_scale_factor,
                                 metadata.top_controls_height,
                                 metadata.top_controls_shown_ratio,
                                 scrollable_viewport_size_dip);

  // ViewAndroid::content_offset() must be in dip.
  float top_content_offset_dip = IsUseZoomForDSFEnabled()
                                     ? top_content_offset / dip_scale
                                     : top_content_offset;
  view_.UpdateFrameInfo({scrollable_viewport_size_dip, top_content_offset_dip});
  bool controls_changed = UpdateControls(
      view_.GetDipScale(), metadata.top_controls_height,
      metadata.top_controls_shown_ratio,
      metadata.top_controls_min_height_offset, metadata.bottom_controls_height,
      metadata.bottom_controls_shown_ratio,
      metadata.bottom_controls_min_height_offset);

  SetContentBackgroundColor(is_transparent ? SK_ColorTRANSPARENT
                                           : root_background_color);

  if (overscroll_controller_) {
    overscroll_controller_->OnFrameMetadataUpdated(
        metadata.page_scale_factor, metadata.device_scale_factor,
        metadata.scrollable_viewport_size, metadata.root_layer_size,
        metadata.root_scroll_offset.value_or(gfx::PointF()),
        metadata.root_overflow_y_hidden);
  }

  // All offsets and sizes except |top_shown_pix| are in dip.
  gesture_listener_manager_->UpdateScrollInfo(
      root_scroll_offset_dip, metadata.page_scale_factor,
      metadata.min_page_scale_factor, metadata.max_page_scale_factor,
      root_layer_size_dip, scrollable_viewport_size_dip, top_content_offset_dip,
      top_shown_pix, controls_changed);
  // This needs to be called after GestureListenerManager::UpdateScrollInfo, as
  // it depends on frame info being updated during the UpdateScrollInfo call.
  auto* wcax = GetWebContentsAccessibilityAndroid();
  if (wcax)
    wcax->UpdateFrameInfo(metadata.page_scale_factor);

  page_scale_ = metadata.page_scale_factor;
  min_page_scale_ = metadata.min_page_scale_factor;
  max_page_scale_ = metadata.max_page_scale_factor;
  current_surface_size_ = metadata.viewport_size_in_pixels;

  // With SurfaceSync we no longer call evict frame on every metadata change. We
  // must still call UpdateWebViewBackgroundColorIfNecessary to maintain the
  // associated background color changes.
  UpdateWebViewBackgroundColorIfNecessary();

  if (metadata.new_vertical_scroll_direction !=
      viz::VerticalScrollDirection::kNull) {
    bool can_scroll = metadata.root_layer_size.height() -
                          metadata.viewport_size_in_pixels.height() >
                      std::numeric_limits<float>::epsilon();
    float scroll_ratio = 0.f;
    if (can_scroll && metadata.root_scroll_offset) {
      scroll_ratio = metadata.root_scroll_offset.value().y() /
                     (metadata.root_layer_size.height() -
                      metadata.viewport_size_in_pixels.height());
    }
    view_.OnVerticalScrollDirectionChanged(
        metadata.new_vertical_scroll_direction ==
            viz::VerticalScrollDirection::kUp,
        scroll_ratio);
  }
}

base::android::ScopedJavaLocalRef<jobject>
RenderWidgetHostViewAndroid::GetJavaObject() {
  if (!obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    obj_.Reset(env, Java_RenderWidgetHostViewImpl_create(
                        env, reinterpret_cast<intptr_t>(this))
                        .obj());
  }
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

bool RenderWidgetHostViewAndroid::IsReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return HasValidFrame();
}

void RenderWidgetHostViewAndroid::DismissTextHandles(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DismissTextHandles();
}

jint RenderWidgetHostViewAndroid::GetBackgroundColor(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  absl::optional<SkColor> color =
      RenderWidgetHostViewAndroid::GetCachedBackgroundColor();
  if (!color)
    return SK_ColorTRANSPARENT;
  return *color;
}

void RenderWidgetHostViewAndroid::ShowContextMenuAtTouchHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint x,
    jint y) {
  if (GetTouchSelectionControllerClientManager()) {
    GetTouchSelectionControllerClientManager()->ShowContextMenu(
        gfx::Point(x, y));
  }
}

void RenderWidgetHostViewAndroid::OnViewportInsetBottomChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              absl::nullopt);
}

void RenderWidgetHostViewAndroid::WriteContentBitmapToDiskAsync(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint width,
    jint height,
    const base::android::JavaParamRef<jstring>& jpath,
    const base::android::JavaParamRef<jobject>& jcallback) {
  base::OnceCallback<void(const SkBitmap&)> result_callback = base::BindOnce(
      &RenderWidgetHostViewAndroid::OnFinishGetContentBitmap,
      weak_ptr_factory_.GetWeakPtr(),
      base::android::ScopedJavaGlobalRef<jobject>(env, obj),
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback),
      base::android::ConvertJavaStringToUTF8(env, jpath));

  CopyFromSurface(gfx::Rect(), gfx::Size(width, height),
                  std::move(result_callback));
}

void RenderWidgetHostViewAndroid::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  const cc::RenderFrameMetadata& metadata =
      host()->render_frame_metadata_provider()->LastRenderFrameMetadata();

  auto activated_local_surface_id =
      metadata.local_surface_id.value_or(viz::LocalSurfaceId());

  if (activated_local_surface_id.is_valid()) {
    // We have received content, ensure that any subsequent navigation allocates
    // a new surface.
    pre_navigation_content_ = true;

    while (!rotation_metrics_.empty()) {
      auto rotation_target = rotation_metrics_.front();
      // Activation from a previous surface before the new rotation has set a
      // viz::LocalSurfaceId.
      if (!rotation_target.second.is_valid())
        break;

      // In most cases the viz::LocalSurfaceId will be the same.
      //
      // However, if there are two cases where this does not occur.
      //
      // Firstly the Renderer may increment the |child_sequence_number| if it
      // needs to also alter visual properties. If so the newer surface would
      // denote the first visual update of the rotation. So its activation time
      // is correct.
      //
      // Otherwise there may be two rotations in close proximity, and one takes
      // too long to present. When this occurs the initial rotation does not
      // display. This newer surface will be the first displayed. Use its
      // activation time for the rotation, as the user would have been blocked
      // on visual updates for that long.
      //
      // We want to know of these long tail rotation times.
      if (activated_local_surface_id.IsSameOrNewerThan(
              rotation_target.second)) {
        // The duration for a rotation encompasses two separate spans of time,
        // depending on whether or not we were `is_showing_` at the start of
        // rotation.
        //
        // For a visible rotation `rotation_target.first` denotes the start of
        // the rotation event handled in BeginRotationBatching.
        //
        // For a hidden rotation we ignore this initial event, as the Renderer
        // can continue to be hidden for a long time. In these cases the
        // `rotation_target.first` denotes when ShowInternal is called.
        //
        // From these, until `activation_time`, we can determine the length of
        // time that the Renderer is visible, until the post rotation surface is
        // first displayed.
        //
        // For hidden rotations, the Renderer may be doing additional, partial,
        // layouts. This is tracked in `hidden_rotation_time_`. This extra work
        // will be removed once `is_surface_sync_throttling_` is the default.
        auto duration =
            activation_time - rotation_target.first + hidden_rotation_time_;
        TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
            "viz", "RenderWidgetHostViewAndroid::RotationEmbed",
            TRACE_ID_LOCAL(rotation_target.second.hash()), activation_time,
            "duration(ms)", duration.InMillisecondsF());
        // Report the total time from the first notification of rotation
        // beginning, until the Renderer has submitted and activated a
        // corresponding surface.
        UMA_HISTOGRAM_TIMES("Android.Rotation.BeginToRendererFrameActivation",
                            duration);
        hidden_rotation_time_ = base::TimeDelta();
        rotation_metrics_.pop_front();
      } else {
        // The embedded surface may have updated the
        // LocalSurfaceId::child_sequence_number while we were updating the
        // parent_sequence_number for `rotation_target`. For example starting
        // from (6, 2) the child advances to (6, 3), and the parent advances to
        // (7, 2). viz::LocalSurfaceId::IsNewerThan will return false in these
        // mixed sequence advancements.
        //
        // Subsequently we would merge the two into (7, 3) which will become the
        // actually submitted surface to Viz.
        //
        // As such we have now received a surface that is not for our target, so
        // we break here and await the next frame from the child.
        break;
      }
    }
    if (rotation_metrics_.empty())
      in_rotation_ = false;
  }
  if (ime_adapter_android_) {
    // We need to first wait for Blink's viewport size to change such that we
    // can correctly scroll to the currently focused input.
    // On Clank, only visible viewport size changes and device viewport size or
    // viewport_size_in_pixels do not change according to the window/view size
    /// change. Only scrollable viewport size changes both for Chrome and
    // WebView.
    ime_adapter_android_->OnRenderFrameMetadataChangedAfterActivation(
        metadata.scrollable_viewport_size);
  }
}

void RenderWidgetHostViewAndroid::OnRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset) {
  if (!gesture_listener_manager_)
    return;
  gfx::PointF root_scroll_offset_dip = root_scroll_offset;
  if (IsUseZoomForDSFEnabled())
    root_scroll_offset_dip.Scale(1 / view_.GetDipScale());
  gesture_listener_manager_->OnRootScrollOffsetChanged(root_scroll_offset_dip);
}

void RenderWidgetHostViewAndroid::Focus() {
  if (view_.HasFocus())
    GotFocus();
  else
    view_.RequestFocus();
}

void RenderWidgetHostViewAndroid::OnFocusInternal() {
  if (overscroll_controller_)
    overscroll_controller_->Enable();
}

void RenderWidgetHostViewAndroid::LostFocusInternal() {
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

bool RenderWidgetHostViewAndroid::HasFocus() {
  return view_.HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() {
  return !using_browser_compositor_ ||
         (delegated_frame_host_ &&
          delegated_frame_host_->CanCopyFromCompositingSurface());
}

void RenderWidgetHostViewAndroid::ShowWithVisibility(
    PageVisibilityState /*page_visibility*/) {
  if (is_showing_)
    return;

  is_showing_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  HideInternal();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // |view_.parent()| being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && view_.parent();
}

void RenderWidgetHostViewAndroid::SelectAroundCaretAck(
    blink::mojom::SelectAroundCaretResultPtr result) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->OnSelectAroundCaretAck(std::move(result));
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() {
  if (!view_.parent())
    return default_bounds_;

  gfx::Size size(view_.GetSize());

  return gfx::Rect(size);
}

gfx::Size RenderWidgetHostViewAndroid::GetVisibleViewportSize() {
  int pinned_bottom_adjust_dps =
      std::max(0, (int)(view_.GetViewportInsetBottom() / view_.GetDipScale()));
  gfx::Rect requested_rect(GetRequestedRendererSize());
  requested_rect.Inset(gfx::Insets(0, 0, pinned_bottom_adjust_dps, 0));
  return requested_rect.size();
}

void RenderWidgetHostViewAndroid::SetInsets(const gfx::Insets& insets) {
  NOTREACHED();
}

gfx::Size RenderWidgetHostViewAndroid::GetCompositorViewportPixelSize() {
  if (!view_.parent()) {
    if (default_bounds_.IsEmpty()) return gfx::Size();

    float scale_factor = view_.GetDipScale();
    return gfx::Size(default_bounds_.right() * scale_factor,
                     default_bounds_.bottom() * scale_factor);
  }

  return view_.GetPhysicalBackingSize();
}

int RenderWidgetHostViewAndroid::GetMouseWheelMinimumGranularity() const {
  auto* window = view_.GetWindowAndroid();
  if (!window)
    return 0;

  // On Android, mouse wheel MotionEvents specify the number of ticks and how
  // many pixels each tick scrolls. This multiplier is specified by device
  // metrics (See WindowAndroid.getMouseWheelScrollFactor) so the minimum
  // granularity will be the size of this tick multiplier.
  return window->mouse_wheel_scroll_factor() / view_.GetDipScale();
}

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& webcursor) {
  view_.OnCursorChanged(webcursor.cursor());
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

// -----------------------------------------------------------------------------
// TextInputManager::Observer implementations.
void RenderWidgetHostViewAndroid::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_change_state) {
  if (!ime_adapter_android_)
    return;

  DCHECK_EQ(text_input_manager_, text_input_manager);
  if (GetTextInputManager()->GetActiveWidget()) {
    ime_adapter_android_->UpdateState(
        *GetTextInputManager()->GetTextInputState());
  } else {
    // If there are no active widgets, the TextInputState.type should be
    // reported as none.
    ime_adapter_android_->UpdateState(ui::mojom::TextInputState());
  }
}

void RenderWidgetHostViewAndroid::OnImeCompositionRangeChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  const TextInputManager::CompositionRangeInfo* info =
      text_input_manager_->GetCompositionRangeInfo();
  if (!info)
    return;

  std::vector<gfx::RectF> character_bounds;
  for (const gfx::Rect& rect : info->character_bounds)
    character_bounds.emplace_back(rect);

  if (ime_adapter_android_)
    ime_adapter_android_->SetCharacterBounds(character_bounds);
}

void RenderWidgetHostViewAndroid::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  if (ime_adapter_android_)
    ime_adapter_android_->CancelComposition();
}

void RenderWidgetHostViewAndroid::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);

  if (!selection_popup_controller_)
    return;

  RenderWidgetHostImpl* focused_widget = GetFocusedWidget();
  if (!focused_widget || !focused_widget->GetView())
    return;

  const TextInputManager::TextSelection& selection =
      *text_input_manager_->GetTextSelection(focused_widget->GetView());

  selection_popup_controller_->OnSelectionChanged(
      base::UTF16ToUTF8(selection.selected_text()));
}

viz::FrameSinkId RenderWidgetHostViewAndroid::GetRootFrameSinkId() {
  if (sync_compositor_)
    return sync_compositor_->GetFrameSinkId();
  if (view_.GetWindowAndroid() && view_.GetWindowAndroid()->GetCompositor())
    return view_.GetWindowAndroid()->GetCompositor()->GetFrameSinkId();
  return viz::FrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewAndroid::GetCurrentSurfaceId() const {
  if (sync_compositor_)
    return sync_compositor_->GetSurfaceId();
  return delegated_frame_host_ ? delegated_frame_host_->SurfaceId()
                               : viz::SurfaceId();
}

bool RenderWidgetHostViewAndroid::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  viz::SurfaceId surface_id = GetCurrentSurfaceId();
  if (!surface_id.is_valid())
    return false;

  // In TransformPointToLocalCoordSpace() there is a Point-to-Pixel conversion,
  // but it is not necessary here because the final target view is responsible
  // for converting before computing the final transform.
  return target_view->TransformPointToLocalCoordSpace(point, surface_id,
                                                      transformed_point);
}

void RenderWidgetHostViewAndroid::SetGestureListenerManager(
    GestureListenerManager* manager) {
  gesture_listener_manager_ = manager;
  UpdateReportAllRootScrolls();
}

void RenderWidgetHostViewAndroid::UpdateReportAllRootScrolls() {
  if (!host())
    return;

  host()->render_frame_metadata_provider()->ReportAllRootScrolls(
      ShouldReportAllRootScrolls());
}

base::WeakPtr<RenderWidgetHostViewAndroid>
RenderWidgetHostViewAndroid::GetWeakPtrAndroid() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventAndroid& event) {
  std::unique_ptr<blink::WebGestureEvent> web_event;
  if (event.scale() < 0.f) {
    // Negative scale indicates zoom reset.
    float delta = min_page_scale_ / page_scale_;
    web_event = ui::CreateWebGestureEventFromGestureEventAndroid(
        ui::GestureEventAndroid(event.type(), event.location(),
                                event.screen_location(), event.time(), delta, 0,
                                0, 0, 0, /*target_viewport*/ false,
                                /*synthetic_scroll*/ false,
                                /*prevent_boosting*/ false));
  } else {
    web_event = ui::CreateWebGestureEventFromGestureEventAndroid(event);
  }
  if (!web_event)
    return false;
  SendGestureEvent(*web_event);
  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event) {
  RecordToolTypeForActionDown(event);

  if (event.GetAction() == ui::MotionEventAndroid::Action::DOWN) {
    if (ime_adapter_android_)
      ime_adapter_android_->UpdateOnTouchDown();
    if (gesture_listener_manager_)
      gesture_listener_manager_->UpdateOnTouchDown();
  }

  if (event.for_touch_handle())
    return OnTouchHandleEvent(event);

  if (!host() || !host()->delegate())
    return false;

  ComputeEventLatencyOSTouchHistograms(event);

  // Receiving any other touch event before the double-tap timeout expires
  // cancels opening the spellcheck menu.
  if (text_suggestion_host_)
    text_suggestion_host_->StopSuggestionMenuTimer();

  // If a browser-based widget consumes the touch event, it's critical that
  // touch event interception be disabled. This avoids issues with
  // double-handling for embedder-detected gestures like side swipe.
  if (OnTouchHandleEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  if (stylus_text_selector_.OnTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
      event, result.moved_beyond_slop_region /* may_cause_scrolling */,
      false /* hovering */);
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined)
    return false;

  ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(this, &web_event,
                                                               latency_info);
  } else {
    host()->ForwardTouchEventWithLatencyInfo(web_event, latency_info);
  }

  // Send a proactive BeginFrame for this vsync to reduce scroll latency for
  // scroll-inducing touch events. Note that Android's Choreographer ensures
  // that BeginFrame requests made during Action::MOVE dispatch will be honored
  // in the same vsync phase.
  if (observing_root_window_ && result.moved_beyond_slop_region) {
    if (sync_compositor_)
      sync_compositor_->RequestOneBeginFrame();
  }
  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchHandleEvent(
    const ui::MotionEvent& event) {
  return touch_selection_controller_ &&
         touch_selection_controller_->WillHandleTouchEvent(event);
}

int RenderWidgetHostViewAndroid::GetTouchHandleHeight() {
  if (!touch_selection_controller_)
    return 0;
  return static_cast<int>(touch_selection_controller_->GetTouchHandleHeight());
}

void RenderWidgetHostViewAndroid::ResetGestureDetection() {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider_.ResetDetection();
    return;
  }

  std::unique_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  if (gesture_provider_.OnTouchEvent(*cancel_event).succeeded) {
    bool causes_scrolling = false;
    ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
    blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
        *cancel_event, causes_scrolling /* may_cause_scrolling */,
        false /* hovering */);
    if (ShouldRouteEvents()) {
      host()->delegate()->GetInputEventRouter()->RouteTouchEvent(
          this, &web_event, latency_info);
    } else {
      host()->ForwardTouchEventWithLatencyInfo(web_event, latency_info);
    }
  }
}

void RenderWidgetHostViewAndroid::OnDidNavigateMainFrameToNewPage() {
  // Move to front only if we are the primary page (we don't want to receive
  // events in the Prerender). GetMainRenderFrameHost() may be null in
  // tests.
  if (view_.parent() &&
      RenderViewHostImpl::From(host())->GetMainRenderFrameHost() &&
      RenderViewHostImpl::From(host())
              ->GetMainRenderFrameHost()
              ->GetLifecycleState() ==
          RenderFrameHost::LifecycleState::kActive) {
    view_.parent()->MoveToFront(&view_);
  }
  ResetGestureDetection();
  if (delegated_frame_host_)
    delegated_frame_host_->OnNavigateToNewPage();
}

void RenderWidgetHostViewAndroid::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void RenderWidgetHostViewAndroid::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  if (ime_adapter_android_)
    ime_adapter_android_->FocusedNodeChanged(is_editable_node);
}

void RenderWidgetHostViewAndroid::RenderProcessGone() {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  host()->render_frame_metadata_provider()->RemoveObserver(this);
  host()->ViewDestroyed();
  UpdateNativeViewTree(nullptr);
  delegated_frame_host_.reset();

  if (GetTextInputManager() && GetTextInputManager()->HasObserver(this))
    GetTextInputManager()->RemoveObserver(this);

  for (auto& observer : destruction_observers_)
    observer.RenderWidgetHostViewDestroyed(this);
  destruction_observers_.Clear();
  // Call this before the derived class is destroyed so that virtual function
  // calls back into `this` still work.
  NotifyObserversAboutShutdown();
  RenderWidgetHostViewBase::Destroy();
  delete this;
}

void RenderWidgetHostViewAndroid::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text) {
  // Tooltips don't make sense on Android.
}

void RenderWidgetHostViewAndroid::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    const gfx::Rect& bounds) {
  // Tooltips don't make sense on Android.
}

void RenderWidgetHostViewAndroid::ClearKeyboardTriggeredTooltip() {
  // Tooltips don't make sense on Android.
}

void RenderWidgetHostViewAndroid::UpdateBackgroundColor() {
  DCHECK(RenderWidgetHostViewBase::GetBackgroundColor());

  SkColor color = *RenderWidgetHostViewBase::GetBackgroundColor();
  view_.OnBackgroundColorChanged(color);
}

bool RenderWidgetHostViewAndroid::HasFallbackSurface() const {
  return delegated_frame_host_ && delegated_frame_host_->HasFallbackSurface();
}

void RenderWidgetHostViewAndroid::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::CopyFromSurface");
  if (!IsSurfaceAvailableForCopy()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  if (!using_browser_compositor_) {
    SynchronousCopyContents(src_subrect, output_size, std::move(callback));
    return;
  }

  DCHECK(delegated_frame_host_);
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, output_size,
      base::BindOnce(
          [](base::OnceCallback<void(const SkBitmap&)> callback,
             base::TimeTicks start_time, const SkBitmap& bitmap) {
            TRACE_EVENT0(
                "cc", "RenderWidgetHostViewAndroid::CopyFromSurface finished");
            // TODO(crbug/1110301): Make the Compositing.CopyFromSurfaceTime
            // histogram obsolete.
            UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                                base::TimeTicks::Now() - start_time);
            std::move(callback).Run(bitmap);
          },
          std::move(callback), start_time));
}

void RenderWidgetHostViewAndroid::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              absl::nullopt);
}

uint32_t RenderWidgetHostViewAndroid::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

bool RenderWidgetHostViewAndroid::CanSynchronizeVisualProperties() {
  // When a rotation begins, the new visual properties are not all notified to
  // RenderWidgetHostViewAndroid at the same time. The process begins when
  // OnSynchronizedDisplayPropertiesChanged is called, and ends with
  // OnPhysicalBackingSizeChanged.
  //
  // During this time there can be upwards of three calls to
  // SynchronizeVisualProperties. Sending each of these separately to the
  // Renderer causes three full re-layouts of the page to occur.
  //
  // We should instead wait for the full set of new visual properties to be
  // available, and deliver them to the Renderer in one single update.
  if (in_rotation_ && is_surface_sync_throttling_)
    return false;
  return true;
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAndroid(host(), &view_));
}

bool RenderWidgetHostViewAndroid::ShouldRouteEvents() const {
  DCHECK(host());
  return host()->delegate() && host()->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewAndroid::UpdateWebViewBackgroundColorIfNecessary() {
  // Android WebView had a bug the BG color was always set to black when
  // fullscreen (see https://crbug.com/961223#c5). As applications came to rely
  // on this behavior, preserve it here.
  if (!using_browser_compositor_ && host()->delegate()->IsFullscreen()) {
    SetContentBackgroundColor(SK_ColorBLACK);
  }
}

void RenderWidgetHostViewAndroid::ClearFallbackSurfaceForCommitPending() {
  delegated_frame_host_->ClearFallbackSurfaceForCommitPending();
  local_surface_id_allocator_.Invalidate();
}

void RenderWidgetHostViewAndroid::ResetFallbackToFirstNavigationSurface() {
  if (delegated_frame_host_)
    delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
}

bool RenderWidgetHostViewAndroid::RequestRepaintForTesting() {
  return SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                     absl::nullopt);
}

void RenderWidgetHostViewAndroid::FrameTokenChangedForSynchronousCompositor(
    uint32_t frame_token,
    const gfx::PointF& root_scroll_offset) {
  if (!viz::FrameTokenGT(frame_token, sync_compositor_last_frame_token_))
    return;
  sync_compositor_last_frame_token_ = frame_token;

  if (host() && frame_token) {
    DCHECK(!using_browser_compositor_);
    // DevTools ScreenCast support for Android WebView. Don't call this if
    // we're currently in SynchronousCopyContents, as this can lead to
    // redundant copies.
    if (!in_sync_copy_contents_) {
      RenderFrameHostImpl* frame_host =
          RenderViewHostImpl::From(host())->GetMainRenderFrameHost();
      if (frame_host && last_render_frame_metadata_) {
        // Update our |root_scroll_offset|, as changes to this value do not
        // trigger a new RenderFrameMetadata, and it may be out of date. This
        // is needed for devtools DOM node selection.
        cc::RenderFrameMetadata updated_metadata = *last_render_frame_metadata_;
        updated_metadata.root_scroll_offset = root_scroll_offset;
        RenderFrameDevToolsAgentHost::SignalSynchronousSwapCompositorFrame(
            frame_host, updated_metadata);
      }
    }
  }
}

void RenderWidgetHostViewAndroid::SetSynchronousCompositorClient(
      SynchronousCompositorClient* client) {
  synchronous_compositor_client_ = client;
  MaybeCreateSynchronousCompositor();
}

void RenderWidgetHostViewAndroid::MaybeCreateSynchronousCompositor() {
  if (!sync_compositor_ && synchronous_compositor_client_) {
    sync_compositor_ = SynchronousCompositorHost::Create(
        this, host()->GetFrameSinkId(), GetHostFrameSinkManager());
    view_.SetCopyOutputCallback(sync_compositor_->GetCopyViewCallback());
    if (renderer_widget_created_)
      sync_compositor_->InitMojo();
  }
}

void RenderWidgetHostViewAndroid::ResetSynchronousCompositor() {
  if (sync_compositor_) {
    view_.SetCopyOutputCallback(ui::ViewAndroid::CopyViewCallback());
    sync_compositor_.reset();
  }
}

void RenderWidgetHostViewAndroid::OnOverscrollRefreshHandlerAvailable() {
  DCHECK(!overscroll_controller_);
  CreateOverscrollControllerIfPossible();
}

bool RenderWidgetHostViewAndroid::SupportsAnimation() const {
  // The synchronous (WebView) compositor does not have a proper browser
  // compositor with which to drive animations.
  return using_browser_compositor_;
}

void RenderWidgetHostViewAndroid::SetNeedsAnimate() {
  DCHECK(view_.GetWindowAndroid());
  DCHECK(using_browser_compositor_);
  view_.GetWindowAndroid()->SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::PointF& position) {
  MoveCaret(gfx::Point(position.x(), position.y()));
}

void RenderWidgetHostViewAndroid::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->MoveRangeSelectionExtent(extent);
}

void RenderWidgetHostViewAndroid::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->SelectBetweenCoordinates(base, extent);
}

void RenderWidgetHostViewAndroid::OnSelectionEvent(
    ui::SelectionEventType event) {
  if (!selection_popup_controller_)
    return;
  DCHECK(touch_selection_controller_);
  // If a selection drag has started, it has taken over the active touch
  // sequence. Immediately cancel gesture detection and any downstream touch
  // listeners (e.g., web content) to communicate this transfer.
  if (event == ui::SELECTION_HANDLES_SHOWN &&
      gesture_provider_.GetCurrentDownEvent()) {
    ResetGestureDetection();
  }
  selection_popup_controller_->OnSelectionEvent(
      event, GetSelectionRect(*touch_selection_controller_));
}

void RenderWidgetHostViewAndroid::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->OnDragUpdate(type, position);
}

ui::TouchSelectionControllerClient*
RenderWidgetHostViewAndroid::GetSelectionControllerClientManagerForTesting() {
  return touch_selection_controller_client_manager_.get();
}

void RenderWidgetHostViewAndroid::SetSelectionControllerClientForTesting(
    std::unique_ptr<ui::TouchSelectionControllerClient> client) {
  touch_selection_controller_client_for_test_.swap(client);

  touch_selection_controller_ = CreateSelectionController(
      touch_selection_controller_client_for_test_.get(), !!view_.parent());
}

std::unique_ptr<ui::TouchHandleDrawable>
RenderWidgetHostViewAndroid::CreateDrawable() {
  if (!using_browser_compositor_) {
    if (!sync_compositor_)
      return nullptr;
    return std::unique_ptr<ui::TouchHandleDrawable>(
        sync_compositor_->client()->CreateDrawable());
  }
  if (!selection_popup_controller_)
    return nullptr;
  return selection_popup_controller_->CreateTouchHandleDrawable();
}

void RenderWidgetHostViewAndroid::DidScroll() {}

void RenderWidgetHostViewAndroid::ShowTouchSelectionContextMenu(
    const gfx::Point& location) {
  host()->ShowContextMenuAtPoint(location, ui::MENU_SOURCE_TOUCH_HANDLE);
}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_dip,
    const gfx::Size& dst_size_in_pixel,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  // Track that we're in this function to avoid repeatedly calling DevTools
  // capture logic.
  base::AutoReset<bool> in_sync_copy_contents(&in_sync_copy_contents_, true);

  // Note: When |src_subrect| is empty, a conversion from the view size must
  // be made instead of using |current_frame_size_|. The latter sometimes also
  // includes extra height for the toolbar UI, which is not intended for
  // capture.
  gfx::Rect valid_src_subrect_in_dips = src_subrect_dip;
  if (valid_src_subrect_in_dips.IsEmpty())
    valid_src_subrect_in_dips = gfx::Rect(GetVisibleViewportSize());
  const gfx::Rect src_subrect_in_pixel = gfx::ToEnclosingRect(
      gfx::ConvertRectToPixels(valid_src_subrect_in_dips, view_.GetDipScale()));

  // TODO(crbug/698974): [BUG] Current implementation does not support read-back
  // of regions that do not originate at (0,0).
  const gfx::Size& input_size_in_pixel = src_subrect_in_pixel.size();
  DCHECK(!input_size_in_pixel.IsEmpty());

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = input_size_in_pixel;
  else
    output_size_in_pixel = dst_size_in_pixel;
  int output_width = output_size_in_pixel.width();
  int output_height = output_size_in_pixel.height();

  if (!sync_compositor_) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(output_width, output_height));
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)output_width / (float)input_size_in_pixel.width(),
      (float)output_height / (float)input_size_in_pixel.height());
  sync_compositor_->DemandDrawSw(&canvas, /*software_canvas=*/true);
  std::move(callback).Run(bitmap);
}

WebContentsAccessibilityAndroid*
RenderWidgetHostViewAndroid::GetWebContentsAccessibilityAndroid() const {
  return web_contents_accessibility_;
}

void RenderWidgetHostViewAndroid::UpdateTouchSelectionController(
    const viz::Selection<gfx::SelectionBound>& selection,
    float page_scale_factor,
    float top_controls_height,
    float top_controls_shown_ratio,
    const gfx::SizeF& scrollable_viewport_size_dip) {
  if (!touch_selection_controller_)
    return;

  DCHECK(touch_selection_controller_client_manager_);
  touch_selection_controller_client_manager_->UpdateClientSelectionBounds(
      selection.start, selection.end, this, nullptr);
  OnUpdateScopedSelectionHandles();

  // Set parameters for adaptive handle orientation.
  gfx::SizeF viewport_size(scrollable_viewport_size_dip);
  viewport_size.Scale(page_scale_factor);
  gfx::RectF viewport_rect(0.0f, top_controls_height * top_controls_shown_ratio,
                           viewport_size.width(), viewport_size.height());
  touch_selection_controller_->OnViewportChanged(viewport_rect);
}

bool RenderWidgetHostViewAndroid::UpdateControls(
    float dip_scale,
    float top_controls_height,
    float top_controls_shown_ratio,
    float top_controls_min_height_offset,
    float bottom_controls_height,
    float bottom_controls_shown_ratio,
    float bottom_controls_min_height_offset) {
  float to_pix = IsUseZoomForDSFEnabled() ? 1.f : dip_scale;
  float top_controls_pix = top_controls_height * to_pix;
  // |top_content_offset| is in physical pixels if --use-zoom-for-dsf is
  // enabled. Otherwise, it is in DIPs.
  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset = top_controls_height * top_controls_shown_ratio;
  float top_shown_pix = top_content_offset * to_pix;
  float top_translate = top_shown_pix - top_controls_pix;
  bool top_changed =
      !cc::MathUtil::IsFloatNearlyTheSame(top_shown_pix, prev_top_shown_pix_);

  float top_min_height_offset_pix = top_controls_min_height_offset * to_pix;
  top_changed |= !cc::MathUtil::IsFloatNearlyTheSame(
      top_min_height_offset_pix, prev_top_controls_min_height_offset_pix_);

  top_changed |= !cc::MathUtil::IsFloatNearlyTheSame(top_controls_pix,
                                                     prev_top_controls_pix_);

  if (top_changed || !controls_initialized_)
    view_.OnTopControlsChanged(top_translate, top_shown_pix,
                               top_min_height_offset_pix);
  prev_top_shown_pix_ = top_shown_pix;
  prev_top_controls_pix_ = top_controls_pix;
  prev_top_controls_translate_ = top_translate;
  prev_top_controls_min_height_offset_pix_ = top_min_height_offset_pix;

  float bottom_controls_pix = bottom_controls_height * to_pix;
  float bottom_shown_pix = bottom_controls_pix * bottom_controls_shown_ratio;
  bool bottom_changed = !cc::MathUtil::IsFloatNearlyTheSame(
      bottom_shown_pix, prev_bottom_shown_pix_);
  float bottom_translate = bottom_controls_pix - bottom_shown_pix;

  float bottom_min_height_offset_pix =
      bottom_controls_min_height_offset * to_pix;
  bottom_changed |= !cc::MathUtil::IsFloatNearlyTheSame(
      bottom_min_height_offset_pix,
      prev_bottom_controls_min_height_offset_pix_);

  if (bottom_changed || !controls_initialized_)
    view_.OnBottomControlsChanged(bottom_translate,
                                  bottom_min_height_offset_pix);
  prev_bottom_shown_pix_ = bottom_shown_pix;
  prev_bottom_controls_translate_ = bottom_translate;
  prev_bottom_controls_min_height_offset_pix_ = bottom_min_height_offset_pix;
  controls_initialized_ = true;
  return top_changed || bottom_changed;
}

void RenderWidgetHostViewAndroid::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  // If the Renderer is updating visual properties, do not block merging and
  // updating on rotation.
  base::AutoReset<bool> in_rotation(&in_rotation_, false);
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              metadata.local_surface_id);
  if (delegated_frame_host_) {
    delegated_frame_host_->SetTopControlsVisibleHeight(
        metadata.top_controls_height * metadata.top_controls_shown_ratio);
  }
}

void RenderWidgetHostViewAndroid::OnFinishGetContentBitmap(
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& callback,
    const std::string& path,
    const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!bitmap.drawsNothing()) {
    auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    base::PostTaskAndReplyWithResult(
        task_runner.get(), FROM_HERE,
        base::BindOnce(&CompressAndSaveBitmap, path, bitmap),
        base::BindOnce(
            &base::android::RunStringCallbackAndroid,
            base::android::ScopedJavaGlobalRef<jobject>(env, callback.obj())));
    return;
  }
  // If readback failed, call empty callback
  base::android::RunStringCallbackAndroid(callback, std::string());
}

void RenderWidgetHostViewAndroid::ShowInternal() {
  bool show = is_showing_ && is_window_activity_started_ && is_window_visible_;
  if (!show)
    return;

  if (!host() || !host()->is_hidden())
    return;

  // Whether evicted or not, we stop batching for rotation in order to get
  // content ready for the new orientation.
  bool rotation_override = in_rotation_;
  base::AutoReset<bool> in_rotation(&in_rotation_, false);

  view_.GetLayer()->SetHideLayerAndSubtree(false);

  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if ((delegated_frame_host_ &&
       delegated_frame_host_->IsPrimarySurfaceEvicted()) ||
      !local_surface_id_allocator_.HasValidLocalSurfaceId()) {
    ui::WindowAndroidCompositor* compositor =
        view_.GetWindowAndroid() ? view_.GetWindowAndroid()->GetCompositor()
                                 : nullptr;
    SynchronizeVisualProperties(
        compositor && compositor->IsDrawingFirstVisibleFrame()
            ? cc::DeadlinePolicy::UseSpecifiedDeadline(
                  ui::DelegatedFrameHostAndroid::FirstFrameTimeoutFrames())
            : cc::DeadlinePolicy::UseDefaultDeadline(),
        absl::nullopt);
    // If we navigated while hidden, we need to update the fallback surface only
    // after we've completed navigation, and embedded the new surface. The
    // |delegated_frame_host_| is always valid when |navigation_while_hidden_|
    // is set to true.
    if (navigation_while_hidden_) {
      navigation_while_hidden_ = false;
      delegated_frame_host_->DidNavigate();
    }
  } else if (rotation_override && is_surface_sync_throttling_) {
    // If a rotation occurred while this was not visible, we need to allocate a
    // new viz::LocalSurfaceId and send the current visual properties to the
    // Renderer. Otherwise there will be no content at all to display.
    //
    // The rotation process will complete after this first surface is displayed.
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                absl::nullopt);
  }

  auto* visible_time_request_trigger = host()->GetVisibleTimeRequestTrigger();
  // The only way this should be null is if there is no RenderWidgetHostView.
  DCHECK(visible_time_request_trigger);
  auto content_to_visible_start_state =
      visible_time_request_trigger->TakeRequest();

  // Only when page is restored from back-forward cache, record content to
  // visible time and for this case no need to check for saved frames to
  // record ContentToVisibleTime.
  bool show_reason_bfcache_restore =
      content_to_visible_start_state
          ? content_to_visible_start_state->show_reason_bfcache_restore
          : false;
  host()->WasShown(show_reason_bfcache_restore
                       ? std::move(content_to_visible_start_state)
                       : blink::mojom::RecordContentToVisibleTimeRequestPtr());

  if (delegated_frame_host_) {
    delegated_frame_host_->WasShown(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), host()->delegate()->IsFullscreen());
  }

  if (view_.parent() && view_.GetWindowAndroid()) {
    StartObservingRootWindow();
    if (sync_compositor_)
      sync_compositor_->RequestOneBeginFrame();
  }

  if (rotation_override) {
    // It's possible that several rotations were all enqueued while this view
    // has hidden. We skip those and update to just the final state.
    size_t skipped_rotations = rotation_metrics_.size() - 1;
    if (skipped_rotations) {
      rotation_metrics_.erase(rotation_metrics_.begin(),
                              rotation_metrics_.begin() + skipped_rotations);
    }
    // If a rotation occurred while we were hidden, we do not want to include
    // all of that idle time in the rotation metrics. However we do want to have
    // the "RotationBegin" tracing event. So end the tracing event, before
    // setting the starting time of the rotation.
    EndRotationBatching();
    rotation_metrics_.begin()->first = base::TimeTicks::Now();
    BeginRotationEmbed();
  }
}

void RenderWidgetHostViewAndroid::HideInternal() {
  DCHECK(!is_showing_ || !is_window_activity_started_ || !is_window_visible_)
      << "Hide called when the widget should be shown.";

  // As we stop visual observations, we clear the current fullscreen state. Once
  // ShowInternal() is invoked the most up to date visual properties will be
  // used.
  fullscreen_rotation_ = false;

  // Only preserve the frontbuffer if the activity was stopped while the
  // window is still visible. This avoids visual artifacts when transitioning
  // between activities.
  bool hide_frontbuffer = is_window_activity_started_ || !is_window_visible_;

  // Only stop observing the root window if the widget has been explicitly
  // hidden and the frontbuffer is being cleared. This allows window visibility
  // notifications to eventually clear the frontbuffer.
  bool stop_observing_root_window = !is_showing_ && hide_frontbuffer;

  if (hide_frontbuffer) {
    view_.GetLayer()->SetHideLayerAndSubtree(true);
    if (delegated_frame_host_)
      delegated_frame_host_->WasHidden();
  }

  if (stop_observing_root_window) {
    DCHECK(!is_showing_);
    StopObservingRootWindow();
  }

  if (!host() || host()->is_hidden())
    return;

  if (overscroll_controller_)
    overscroll_controller_->Disable();

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host()->WasHidden();
}

void RenderWidgetHostViewAndroid::StartObservingRootWindow() {
  DCHECK(view_.parent());
  DCHECK(view_.GetWindowAndroid());
  DCHECK(is_showing_);
  if (observing_root_window_)
    return;

  observing_root_window_ = true;
  view_.GetWindowAndroid()->AddObserver(this);

  ui::WindowAndroidCompositor* compositor =
      view_.GetWindowAndroid()->GetCompositor();
  if (compositor) {
    delegated_frame_host_->AttachToCompositor(compositor);
  }

  OnUpdateScopedSelectionHandles();
}

void RenderWidgetHostViewAndroid::StopObservingRootWindow() {
  if (!(view_.GetWindowAndroid())) {
    DCHECK(!observing_root_window_);
    return;
  }

  if (!observing_root_window_)
    return;

  // Reset window state variables to their defaults.
  is_window_activity_started_ = true;
  is_window_visible_ = true;
  observing_root_window_ = false;
  OnUpdateScopedSelectionHandles();
  view_.GetWindowAndroid()->RemoveObserver(this);
  // If the DFH has already been destroyed, it will have cleaned itself up.
  // This happens in some WebView cases.
  if (delegated_frame_host_)
    delegated_frame_host_->DetachFromCompositor();
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  bool needs_animate = false;
  if (overscroll_controller_) {
    needs_animate |=
        overscroll_controller_->Animate(frame_time, view_.parent()->GetLayer());
  }
  // TODO(wjmaclean): Investigate how animation here does or doesn't affect
  // an OOPIF client.
  if (touch_selection_controller_)
    needs_animate |= touch_selection_controller_->Animate(frame_time);
  return needs_animate;
}

void RenderWidgetHostViewAndroid::RequestDisallowInterceptTouchEvent() {
  if (view_.parent())
    view_.RequestDisallowInterceptTouchEvent();
}

void RenderWidgetHostViewAndroid::TransformPointToRootSurface(
    gfx::PointF* point) {
  if (!host()->delegate())
    return;
  RenderViewHostDelegateView* rvh_delegate_view =
      host()->delegate()->GetDelegateView();
  if (rvh_delegate_view->DoBrowserControlsShrinkRendererSize())
    *point += gfx::Vector2d(0, rvh_delegate_view->GetTopControlsHeight());
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  gesture_provider_.OnTouchEventAck(
      touch.event.unique_touch_event_id, event_consumed,
      InputEventResultStateIsSetNonBlocking(ack_result));
  if (touch.event.touch_start_or_first_touch_move && event_consumed &&
      host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()
        ->delegate()
        ->GetInputEventRouter()
        ->OnHandledTouchStartOrFirstTouchMove(
            touch.event.unique_touch_event_id);
  }
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (overscroll_controller_)
    overscroll_controller_->OnGestureEventAck(event, ack_result);
  mouse_wheel_phase_handler_.GestureEventAck(event, ack_result);

  ForwardTouchpadZoomEventIfNecessary(event, ack_result);

  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  if (gesture_listener_manager_)
    gesture_listener_manager_->GestureEventAck(event, ack_result);

  HandleSwipeToMoveCursorGestureAck(event);
}

void RenderWidgetHostViewAndroid::ChildDidAckGestureEvent(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (gesture_listener_manager_)
    gesture_listener_manager_->GestureEventAck(event, ack_result);
}

blink::mojom::InputEventResultState
RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (overscroll_controller_ &&
      blink::WebInputEvent::IsGestureEventType(input_event.GetType())) {
    blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    if (overscroll_controller_->WillHandleGestureEvent(gesture_event)) {
      // Terminate an active fling when a GSU generated from the fling progress
      // (GSU with inertial state) is consumed by the overscroll_controller_ and
      // overscrolling mode is not |OVERSCROLL_NONE|. The early fling
      // termination generates a GSE which completes the overscroll action.
      if (gesture_event.GetType() ==
              blink::WebInputEvent::Type::kGestureScrollUpdate &&
          gesture_event.data.scroll_update.inertial_phase ==
              blink::WebGestureEvent::InertialPhaseState::kMomentum) {
        host_->StopFling();
      }

      return blink::mojom::InputEventResultState::kConsumed;
    }
  }

  if (gesture_listener_manager_ &&
      gesture_listener_manager_->FilterInputEvent(input_event)) {
    return blink::mojom::InputEventResultState::kConsumed;
  }

  if (!host())
    return blink::mojom::InputEventResultState::kNotConsumed;

  if (input_event.GetType() == blink::WebInputEvent::Type::kGestureTapDown ||
      input_event.GetType() == blink::WebInputEvent::Type::kTouchStart) {
    GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */,
                             base::BindOnce(&WakeUpGpu));
  }

  return blink::mojom::InputEventResultState::kNotConsumed;
}

blink::mojom::PointerLockResult RenderWidgetHostViewAndroid::LockMouse(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnsupportedOptions;
}

blink::mojom::PointerLockResult RenderWidgetHostViewAndroid::ChangeMouseLock(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnsupportedOptions;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host())
    return;

  RenderWidgetHostImpl* target_host = host();

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host()->delegate())
    target_host = host()->delegate()->GetFocusedRenderWidgetHost(host());
  if (!target_host)
    return;

  // Receiving a key event before the double-tap timeout expires cancels opening
  // the spellcheck menu. If the suggestion menu is open, we close the menu.
  if (text_suggestion_host_)
    text_suggestion_host_->OnKeyEvent();

  ui::LatencyInfo latency_info;
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::Type::kChar) {
    latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  }
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  target_host->ForwardKeyboardEventWithLatencyInfo(event, latency_info);
}

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const ui::MotionEventAndroid& motion_event,
    int action_button) {
  blink::WebInputEvent::Type webMouseEventType =
      ui::ToWebMouseEventType(motion_event.GetAction());

  if (webMouseEventType == blink::WebInputEvent::Type::kUndefined)
    return;

  if (webMouseEventType == blink::WebInputEvent::Type::kMouseDown)
    UpdateMouseState(action_button, motion_event.GetX(0), motion_event.GetY(0));

  int click_count = 0;

  if (webMouseEventType == blink::WebInputEvent::Type::kMouseDown ||
      webMouseEventType == blink::WebInputEvent::Type::kMouseUp)
    click_count = (action_button == ui::MotionEventAndroid::BUTTON_PRIMARY)
                      ? left_click_count_
                      : 1;

  blink::WebMouseEvent mouse_event = WebMouseEventBuilder::Build(
      motion_event, webMouseEventType, click_count, action_button);

  if (!host() || !host()->delegate())
    return;

  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteMouseEvent(
        this, &mouse_event, ui::LatencyInfo());
  } else {
    host()->ForwardMouseEvent(mouse_event);
  }
}

void RenderWidgetHostViewAndroid::UpdateMouseState(int action_button,
                                                   float mousedown_x,
                                                   float mousedown_y) {
  if (action_button != ui::MotionEventAndroid::BUTTON_PRIMARY) {
    // Reset state if middle or right button was pressed.
    left_click_count_ = 0;
    prev_mousedown_timestamp_ = base::TimeTicks();
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeDelta time_delay = current_time - prev_mousedown_timestamp_;
  const gfx::Point mousedown_point(mousedown_x, mousedown_y);
  const float distance_squared =
      (mousedown_point - prev_mousedown_point_).LengthSquared();
  if (left_click_count_ > 2 || time_delay > kClickCountInterval ||
      distance_squared > kClickCountRadiusSquaredDIP) {
    left_click_count_ = 0;
  }
  left_click_count_++;
  prev_mousedown_timestamp_ = current_time;
  prev_mousedown_point_ = mousedown_point;
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (!host() || !host()->delegate())
    return;

  ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  blink::WebMouseWheelEvent wheel_event(event);
  bool should_route_events = ShouldRouteEvents();
  mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
      wheel_event, should_route_events);

  if (should_route_events) {
    host()->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        this, &wheel_event, latency_info);
  } else {
    host()->ForwardWheelEventWithLatencyInfo(wheel_event, latency_info);
  }
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if (!host() || !host()->delegate() ||
      event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

  // We let the touch selection controller see gesture events here, since they
  // may be routed and not make it to FilterInputEvent().
  if (touch_selection_controller_ &&
      event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    switch (event.GetType()) {
      case blink::WebInputEvent::Type::kGestureLongPress:
        touch_selection_controller_->HandleLongPressEvent(
            event.TimeStamp(), event.PositionInWidget());
        break;

      case blink::WebInputEvent::Type::kGestureTap:
        touch_selection_controller_->HandleTapEvent(event.PositionInWidget(),
                                                    event.data.tap.tap_count);
        break;

      case blink::WebInputEvent::Type::kGestureScrollBegin:
        touch_selection_controller_->OnScrollBeginEvent();
        break;

      default:
        break;
    }
  }

  ui::LatencyInfo latency_info =
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event);
  if (event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
      // If there is a current scroll going on and a new scroll that isn't
      // wheel based, send a synthetic wheel event with kPhaseEnded to cancel
      // the current scroll.
      mouse_wheel_phase_handler_.DispatchPendingWheelEndEvent();
    } else if (event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
      // Make sure that the next wheel event will have phase = |kPhaseBegan|.
      // This is for maintaining the correct phase info when some of the wheel
      // events get ignored while a touchscreen scroll is going on.
      mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
    }

  } else if (event.GetType() ==
                 blink::WebInputEvent::Type::kGestureFlingStart &&
             event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
    // Ignore the pending wheel end event to avoid sending a wheel event with
    // kPhaseEnded before a GFS.
    mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
  }
  if (ShouldRouteEvents()) {
    blink::WebGestureEvent gesture_event(event);
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &gesture_event, latency_info);
  } else {
    host()->ForwardGestureEventWithLatencyInfo(event, latency_info);
  }
}

bool RenderWidgetHostViewAndroid::ShowSelectionMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
  if (!selection_popup_controller_ || is_in_vr_)
    return false;

  return selection_popup_controller_->ShowSelectionMenu(
      render_frame_host, params, GetTouchHandleHeight());
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host() && host()->delegate())
    host()->delegate()->MoveCaret(point);
}

void RenderWidgetHostViewAndroid::DismissTextHandles() {
  if (touch_selection_controller_)
    touch_selection_controller_->HideAndDisallowShowingAutomatically();
}

void RenderWidgetHostViewAndroid::SetTextHandlesTemporarilyHidden(
    bool hide_handles) {
  if (!touch_selection_controller_ ||
      handles_hidden_by_selection_ui_ == hide_handles)
    return;
  handles_hidden_by_selection_ui_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

absl::optional<SkColor>
RenderWidgetHostViewAndroid::GetCachedBackgroundColor() {
  return RenderWidgetHostViewBase::GetBackgroundColor();
}

void RenderWidgetHostViewAndroid::SetIsInVR(bool is_in_vr) {
  if (is_in_vr_ == is_in_vr)
    return;
  is_in_vr_ = is_in_vr;
  // TODO(crbug.com/851054): support touch selection handles in VR.
  SetTextHandlesHiddenInternal();

  gesture_provider_.UpdateConfig(ui::GetGestureProviderConfig(
      is_in_vr_ ? ui::GestureProviderConfigType::CURRENT_PLATFORM_VR
                : ui::GestureProviderConfigType::CURRENT_PLATFORM,
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})));
}

bool RenderWidgetHostViewAndroid::IsInVR() const {
  return is_in_vr_;
}

void RenderWidgetHostViewAndroid::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (sync_compositor_)
    sync_compositor_->DidOverscroll(params);

  if (!view_.parent() || !is_showing_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnOverscrolled(params);
}

void RenderWidgetHostViewAndroid::DidStopFlinging() {
  if (!gesture_listener_manager_)
    return;
  gesture_listener_manager_->DidStopFlinging();
}

const viz::FrameSinkId& RenderWidgetHostViewAndroid::GetFrameSinkId() const {
  if (!delegated_frame_host_)
    return viz::FrameSinkIdAllocator::InvalidFrameSinkId();

  return delegated_frame_host_->GetFrameSinkId();
}

void RenderWidgetHostViewAndroid::UpdateNativeViewTree(
    gfx::NativeView parent_native_view) {
  bool will_build_tree = parent_native_view != nullptr;
  bool has_view_tree = view_.parent() != nullptr;

  // Allows same parent view to be set again.
  DCHECK(!will_build_tree || !has_view_tree ||
         parent_native_view == view_.parent());

  StopObservingRootWindow();

  bool resize = false;
  if (will_build_tree != has_view_tree) {
    touch_selection_controller_.reset();
    if (has_view_tree) {
      view_.RemoveObserver(this);
      view_.RemoveFromParent();
      view_.GetLayer()->RemoveFromParent();
    }
    if (will_build_tree) {
      view_.AddObserver(this);
      parent_native_view->AddChild(&view_);
      parent_native_view->GetLayer()->AddChild(view_.GetLayer());
    }

    // TODO(yusufo) : Get rid of the below conditions and have a better handling
    // for resizing after crbug.com/628302 is handled.
    bool is_size_initialized = !will_build_tree ||
                               view_.GetSize().width() != 0 ||
                               view_.GetSize().height() != 0;
    if (has_view_tree || is_size_initialized)
      resize = true;
    has_view_tree = will_build_tree;
  }

  if (!has_view_tree) {
    ResetSynchronousCompositor();
    return;
  }
  // Parent native view can become null and then later non-null again, if
  // WebContents swaps away from this, and then later back to it. Need to
  // ensure SynchronousCompositor is recreated in this case.
  MaybeCreateSynchronousCompositor();

  // Force an initial update of screen infos so the default RWHVBase value
  // is not used.
  // TODO(enne): figure out a more straightforward init path for screen infos.
  UpdateScreenInfo();

  if (is_showing_ && view_.GetWindowAndroid())
    StartObservingRootWindow();

  if (resize) {
    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseSpecifiedDeadline(
            ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames()),
        absl::nullopt);
  }

  if (!touch_selection_controller_) {
    ui::TouchSelectionControllerClient* client =
        touch_selection_controller_client_manager_.get();
    if (touch_selection_controller_client_for_test_)
      client = touch_selection_controller_client_for_test_.get();

    touch_selection_controller_ = CreateSelectionController(client, true);
  }

  CreateOverscrollControllerIfPossible();
}

bool RenderWidgetHostViewAndroid::ShouldReportAllRootScrolls() {
  // In order to provide support for onScrollOffsetOrExtentChanged()
  // GestureListenerManager needs root-scroll-offsets. This is only necessary
  // if a GestureStateListenerWithScroll is added.
  return web_contents_accessibility_ != nullptr ||
         (gesture_listener_manager_ &&
          gesture_listener_manager_->has_listeners_attached());
}

MouseWheelPhaseHandler*
RenderWidgetHostViewAndroid::GetMouseWheelPhaseHandler() {
  return &mouse_wheel_phase_handler_;
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewAndroid::GetTouchSelectionControllerClientManager() {
  return touch_selection_controller_client_manager_.get();
}

const viz::LocalSurfaceId& RenderWidgetHostViewAndroid::GetLocalSurfaceId()
    const {
  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void RenderWidgetHostViewAndroid::OnRendererWidgetCreated() {
  renderer_widget_created_ = true;
  if (sync_compositor_)
    sync_compositor_->InitMojo();
}

bool RenderWidgetHostViewAndroid::OnMouseEvent(
    const ui::MotionEventAndroid& event) {
  // Ignore ACTION_HOVER_ENTER & ACTION_HOVER_EXIT because every mouse-down on
  // Android follows a hover-exit and is followed by a hover-enter.
  // https://crbug.com/715114 filed on distinguishing actual hover
  // enter/exit from these bogus ones.
  auto action = event.GetAction();
  if (action == ui::MotionEventAndroid::Action::HOVER_ENTER ||
      action == ui::MotionEventAndroid::Action::HOVER_EXIT) {
    return false;
  }

  RecordToolTypeForActionDown(event);
  SendMouseEvent(event, event.GetActionButton());
  return true;
}

bool RenderWidgetHostViewAndroid::OnMouseWheelEvent(
    const ui::MotionEventAndroid& event) {
  SendMouseWheelEvent(WebMouseWheelEventBuilder::Build(event));
  return true;
}

void RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::ET_GESTURE_PINCH_BEGIN ||
       gesture.type() == ui::ET_GESTURE_PINCH_UPDATE ||
       gesture.type() == ui::ET_GESTURE_PINCH_END) &&
      !IsPinchToZoomEnabled()) {
    return;
  }

  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.GetType() == blink::WebInputEvent::Type::kGestureTap &&
      web_gesture.GetModifiers() == blink::WebInputEvent::kShiftKey) {
    web_gesture.SetModifiers(blink::WebInputEvent::kNoModifiers);
  }
  SendGestureEvent(web_gesture);
}

bool RenderWidgetHostViewAndroid::RequiresDoubleTapGestureEvents() const {
  return true;
}

void RenderWidgetHostViewAndroid::OnPhysicalBackingSizeChanged(
    absl::optional<base::TimeDelta> deadline_override) {
  // We may need to update the background color to match pre-surface-sync
  // behavior of EvictFrameIfNecessary.
  UpdateWebViewBackgroundColorIfNecessary();
  int64_t deadline_in_frames =
      deadline_override ? ui::DelegatedFrameHostAndroid::TimeDeltaToFrames(
                              deadline_override.value())
                        : ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames();

  // Cache the current rotation state so that we can start embedding with the
  // latest visual properties from SynchronizeVisualProperties().
  bool in_rotation = in_rotation_;
  if (in_rotation)
    EndRotationBatching();
  // When exiting fullscreen it is possible that
  // OnSynchronizedDisplayPropertiesChanged is either called out-of-order, or
  // not at all. If so we treat this as the start of the rotation.
  //
  // TODO(jonross): Build a unified screen state observer to replace all of the
  // individual signals used by RenderWidgetHostViewAndroid.
  if (fullscreen_rotation_ && !host()->delegate()->IsFullscreen())
    BeginRotationBatching();
  SynchronizeVisualProperties(
      cc::DeadlinePolicy::UseSpecifiedDeadline(deadline_in_frames),
      absl::nullopt);
  if (in_rotation)
    BeginRotationEmbed();
}

void RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged(bool visible) {
  TRACE_EVENT1("browser",
               "RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged",
               "visible", visible);
  DCHECK(observing_root_window_);

  // Don't early out if visibility hasn't changed and visible. This is necessary
  // as OnDetachedFromWindow() sets |is_window_visible_| to true, so that this
  // may be called when ShowInternal() needs to be called.
  if (is_window_visible_ == visible && !visible)
    return;

  is_window_visible_ = visible;

  if (visible)
    ShowInternal();
  else
    HideInternal();
}

void RenderWidgetHostViewAndroid::OnAttachedToWindow() {
  if (!view_.parent())
    return;

  if (is_showing_)
    StartObservingRootWindow();
  DCHECK(view_.GetWindowAndroid());
  if (view_.GetWindowAndroid()->GetCompositor())
    OnAttachCompositor();
}

void RenderWidgetHostViewAndroid::OnDetachedFromWindow() {
  StopObservingRootWindow();
  OnDetachCompositor();
}

void RenderWidgetHostViewAndroid::OnAttachCompositor() {
  DCHECK(view_.parent());
  CreateOverscrollControllerIfPossible();
  if (observing_root_window_ && using_browser_compositor_) {
    ui::WindowAndroidCompositor* compositor =
        view_.GetWindowAndroid()->GetCompositor();
    delegated_frame_host_->AttachToCompositor(compositor);
  }
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(view_.parent());
  overscroll_controller_.reset();
  if (using_browser_compositor_)
    delegated_frame_host_->DetachFromCompositor();
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnActivityStopped() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStopped");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = false;
  HideInternal();
}

void RenderWidgetHostViewAndroid::OnActivityStarted() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStarted");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenForStylus(
    bool hide_handles) {
  if (!touch_selection_controller_ || handles_hidden_by_stylus_ == hide_handles)
    return;
  handles_hidden_by_stylus_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenInternal() {
  if (!touch_selection_controller_)
    return;
  // TODO(crbug.com/851054): support touch selection handles in VR.
  touch_selection_controller_->SetTemporarilyHidden(
      is_in_vr_ || handles_hidden_by_stylus_ ||
      handles_hidden_by_selection_ui_);
}

void RenderWidgetHostViewAndroid::OnStylusSelectBegin(float x0,
                                                      float y0,
                                                      float x1,
                                                      float y1) {
  SetTextHandlesHiddenForStylus(true);
  // TODO(ajith.v) Refactor the event names as this is not really handle drag,
  // but currently we use same for long press drag selection as well.
  OnSelectionEvent(ui::SELECTION_HANDLE_DRAG_STARTED);
  SelectBetweenCoordinates(gfx::PointF(x0, y0), gfx::PointF(x1, y1));
}

void RenderWidgetHostViewAndroid::OnStylusSelectUpdate(float x, float y) {
  MoveRangeSelectionExtent(gfx::PointF(x, y));
}

void RenderWidgetHostViewAndroid::OnStylusSelectEnd(float x, float y) {
  SetTextHandlesHiddenForStylus(false);
  // TODO(ajith.v) Refactor the event names as this is not really handle drag,
  // but currently we use same for long press drag selection as well.
  OnSelectionEvent(ui::SELECTION_HANDLE_DRAG_STOPPED);
}

void RenderWidgetHostViewAndroid::OnStylusSelectTap(base::TimeTicks time,
                                                    float x,
                                                    float y) {
  // Treat the stylus tap as a long press, activating either a word selection or
  // context menu depending on the targetted content.
  blink::WebGestureEvent long_press = WebGestureEventBuilder::Build(
      blink::WebInputEvent::Type::kGestureLongPress, time, x, y);
  SendGestureEvent(long_press);
}

void RenderWidgetHostViewAndroid::ComputeEventLatencyOSTouchHistograms(
      const ui::MotionEvent& event) {
  base::TimeTicks event_time = event.GetEventTime();
  base::TimeTicks current_time = base::TimeTicks::Now();
  ui::EventType event_type;
  switch (event.GetAction()) {
    case ui::MotionEvent::Action::DOWN:
    case ui::MotionEvent::Action::POINTER_DOWN:
      event_type = ui::ET_TOUCH_PRESSED;
      break;
    case ui::MotionEvent::Action::MOVE:
      event_type = ui::ET_TOUCH_MOVED;
      break;
    case ui::MotionEvent::Action::UP:
    case ui::MotionEvent::Action::POINTER_UP:
      event_type = ui::ET_TOUCH_RELEASED;
      break;
    default:
      return;
  }
  ui::ComputeEventLatencyOS(event_type, event_time, current_time);
}

void RenderWidgetHostViewAndroid::CreateOverscrollControllerIfPossible() {
  // an OverscrollController is already set
  if (overscroll_controller_)
    return;

  RenderWidgetHostDelegate* delegate = host()->delegate();
  if (!delegate)
    return;

  RenderViewHostDelegateView* delegate_view = delegate->GetDelegateView();
  // render_widget_host_unittest.cc uses an object called
  // MockRenderWidgetHostDelegate that does not have a DelegateView
  if (!delegate_view)
    return;

  ui::OverscrollRefreshHandler* overscroll_refresh_handler =
      delegate_view->GetOverscrollRefreshHandler();
  if (!overscroll_refresh_handler)
    return;

  if (!view_.parent())
    return;

  // If window_android is null here, this is bad because we don't listen for it
  // being set, so we won't be able to construct the OverscrollController at the
  // proper time.
  ui::WindowAndroid* window_android = view_.GetWindowAndroid();
  if (!window_android)
    return;

  ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
  if (!compositor)
    return;

  overscroll_controller_ = std::make_unique<OverscrollControllerAndroid>(
      overscroll_refresh_handler, compositor, view_.GetDipScale());
}

void RenderWidgetHostViewAndroid::SetOverscrollControllerForTesting(
    ui::OverscrollRefreshHandler* overscroll_refresh_handler) {
  overscroll_controller_ = std::make_unique<OverscrollControllerAndroid>(
      overscroll_refresh_handler, view_.GetWindowAndroid()->GetCompositor(),
      view_.GetDipScale());
}

void RenderWidgetHostViewAndroid::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  CopyBackgroundColorIfPresentFrom(*view);

  RenderWidgetHostViewAndroid* view_android =
      static_cast<RenderWidgetHostViewAndroid*>(view);
  if (!delegated_frame_host_ || !view_android->delegated_frame_host_)
    return;
  delegated_frame_host_->TakeFallbackContentFrom(
      view_android->delegated_frame_host_.get());
  host()->GetContentRenderingTimeoutFrom(view_android->host());
}

void RenderWidgetHostViewAndroid::OnSynchronizedDisplayPropertiesChanged(
    bool rotation) {
  if (rotation) {
    if (!in_rotation_) {
      // If this is a new rotation confirm the rotation state to prepare for
      // future exiting. As OnSynchronizedDisplayPropertiesChanged is not always
      // called when exiting.
      // TODO(jonross): Build a unified screen state observer to replace all of
      // the individual signals used by RenderWidgetHostViewAndroid.
      fullscreen_rotation_ = host()->delegate()->IsFullscreen() && is_showing_;
      BeginRotationBatching();
    } else if (fullscreen_rotation_) {
      // If exiting fullscreen triggers a rotation, begin embedding now, as we
      // have previously had OnPhysicalBackingSizeChanged called.
      fullscreen_rotation_ = false;
      EndRotationBatching();
      BeginRotationEmbed();
    }
  }
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              absl::nullopt);
}

absl::optional<SkColor> RenderWidgetHostViewAndroid::GetBackgroundColor() {
  return default_background_color_;
}

void RenderWidgetHostViewAndroid::DidNavigate() {
  if (!delegated_frame_host_) {
    RenderWidgetHostViewBase::DidNavigate();
    return;
  }
  if (!is_showing_) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    local_surface_id_allocator_.Invalidate();
    navigation_while_hidden_ = true;
  } else {
    // TODO(jonross): This was a legacy optimization to not perform too many
    // Surface Synchronization iterations for the first navigation. However we
    // currently are performing 5 full synchornizations before navigation
    // completes anyways. So we need to re-do RWHVA setup.
    // (https://crbug.com/1245652)
    //
    // In the interim we will not allocate a new Surface as long as the Renderer
    // has yet to produce any content. If we have existing content always
    // allocate a new surface, as the content will be from a pre-navigation
    // source.
    if (!pre_navigation_content_) {
      SynchronizeVisualProperties(
          cc::DeadlinePolicy::UseExistingDeadline(),
          local_surface_id_allocator_.GetCurrentLocalSurfaceId());
    } else {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  absl::nullopt);
    }
    // Only notify of navigation once a surface has been embedded.
    delegated_frame_host_->DidNavigate();
  }
  pre_navigation_content_ = true;
}

WebContentsAccessibility*
RenderWidgetHostViewAndroid::GetWebContentsAccessibility() {
  return web_contents_accessibility_;
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewAndroid::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &RenderWidgetHostViewAndroid::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

display::ScreenInfo RenderWidgetHostViewAndroid::GetScreenInfo() const {
  bool use_window_wide_color_gamut =
      GetContentClient()->browser()->GetWideColorGamutHeuristic() ==
      ContentBrowserClient::WideColorGamutHeuristic::kUseWindow;
  auto* window = view_.GetWindowAndroid();
  if (!window || !use_window_wide_color_gamut) {
    return RenderWidgetHostViewBase::GetScreenInfo();
  }
  display::ScreenInfo screen_info;
  display::DisplayUtil::DisplayToScreenInfo(
      &screen_info, window->GetDisplayWithWindowColorSpace());
  return screen_info;
}

std::vector<std::unique_ptr<ui::TouchEvent>>
RenderWidgetHostViewAndroid::ExtractAndCancelActiveTouches() {
  ResetGestureDetection();
  return {};
}

void RenderWidgetHostViewAndroid::TransferTouches(
    const std::vector<std::unique_ptr<ui::TouchEvent>>& touches) {
  // Touch transfer for Android is not implemented in content/.
}

absl::optional<DisplayFeature>
RenderWidgetHostViewAndroid::GetDisplayFeature() {
  gfx::Size view_size(view_.GetSize());
  if (view_size.IsEmpty())
    return absl::nullopt;

  // On Android, the display feature is exposed as a rectangle as a generic
  // concept. Here in the content layer, we translate that to a more
  // constrained concept, see content::DisplayFeature.
  absl::optional<gfx::Rect> display_feature_rect = view_.GetDisplayFeature();
  if (!display_feature_rect)
    return absl::nullopt;

  // The display feature and view location are both provided in device pixels,
  // relative to the window. Convert this to DIP and view relative coordinates,
  // first by applying the scale, converting the display feature to view
  // relative coordinates, then intersect with the view bounds rect.
  // the convert to view-relative coordinates.
  float dip_scale = 1 / view_.GetDipScale();
  gfx::Point view_location = view_.GetLocationOfContainerViewInWindow();
  view_location = gfx::ScaleToRoundedPoint(view_location, dip_scale);
  gfx::Rect transformed_display_feature =
      gfx::ScaleToRoundedRect(*display_feature_rect, dip_scale);

  transformed_display_feature.Offset(-view_location.x(), -view_location.y());
  transformed_display_feature.Intersect(gfx::Rect(view_size));

  DisplayFeature display_feature;
  if (transformed_display_feature.x() == 0 &&
      transformed_display_feature.width() == view_size.width()) {
    // A horizontal display feature covers the view's width and starts at
    // an x-offset of 0.
    display_feature = {DisplayFeature::Orientation::kHorizontal,
                       transformed_display_feature.y(),
                       transformed_display_feature.height()};
  } else if (transformed_display_feature.y() == 0 &&
             transformed_display_feature.height() == view_size.height()) {
    // A vertical display feature covers the view's height and starts at
    // a y-offset of 0.
    display_feature = {DisplayFeature::Orientation::kVertical,
                       transformed_display_feature.x(),
                       transformed_display_feature.width()};
  } else {
    return absl::nullopt;
  }

  return display_feature;
}

void RenderWidgetHostViewAndroid::SetDisplayFeatureForTesting(
    const DisplayFeature* display_feature) {
  // RenderWidgetHostViewAndroid display feature mocking should be done via
  // TestViewAndroidDelegate instead - see MockDisplayFeature.
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr) {
  // ShowWithVisibility calls ShowInternal instead of
  // RenderWidgetHostViewBase::OnShowWithPageVisibility so nothing should
  // call this.
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::RequestPresentationTimeFromHostOrDelegate(
    blink::mojom::RecordContentToVisibleTimeRequestPtr) {
  // ShowWithVisibility calls ShowInternal instead of
  // RenderWidgetHostViewBase::OnShowWithPageVisibility so nothing should
  // call this.
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::
    CancelPresentationTimeRequestForHostAndDelegate() {
  // ShowWithVisibility calls ShowInternal instead of
  // RenderWidgetHostViewBase::OnShowWithPageVisibility so nothing should
  // call this.
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::HandleSwipeToMoveCursorGestureAck(
    const blink::WebGestureEvent& event) {
  if (!touch_selection_controller_ || !selection_popup_controller_) {
    swipe_to_move_cursor_activated_ = false;
    return;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin: {
      if (!event.data.scroll_begin.cursor_control)
        break;
      swipe_to_move_cursor_activated_ = true;
      touch_selection_controller_->OnSwipeToMoveCursorBegin();
      OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STARTED);
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollUpdate: {
      if (!swipe_to_move_cursor_activated_)
        break;
      gfx::RectF rect = touch_selection_controller_->GetRectBetweenBounds();
      // Suppress this when the input is not focused, in which case rect will be
      // 0x0.
      if (rect.width() != 0.f || rect.height() != 0.f) {
        selection_popup_controller_->OnDragUpdate(
            ui::TouchSelectionDraggable::Type::kNone,
            gfx::PointF(event.PositionInWidget().x(), rect.right_center().y()));
      }
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      if (!swipe_to_move_cursor_activated_)
        break;
      swipe_to_move_cursor_activated_ = false;
      touch_selection_controller_->OnSwipeToMoveCursorEnd();
      OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STOPPED);
      break;
    }
    default:
      break;
  }
}

void RenderWidgetHostViewAndroid::WasEvicted() {
  // Eviction can occur when the CompositorFrameSink has changed. This can
  // occur either from a lost connection, as well as from the initial conneciton
  // upon creating RenderWidgetHostViewAndroid. When this occurs while visible
  // a new LocalSurfaceId should be generated. If eviction occurs while not
  // visible, then the new LocalSurfaceId can be allocated upon the next Show.
  if (is_showing_) {
    local_surface_id_allocator_.GenerateId();
    // Guarantee that the new LocalSurfaceId is propagated. Rather than relying
    // upon calls to Show() and OnDidUpdateVisualPropertiesComplete(). As there
    // is no guarantee that they will occur after the eviction.
    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseExistingDeadline(),
        local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  } else {
    local_surface_id_allocator_.Invalidate();
  }
}

void RenderWidgetHostViewAndroid::OnUpdateScopedSelectionHandles() {
  if (!observing_root_window_ ||
      !touch_selection_controller_client_manager_->has_active_selection()) {
    scoped_selection_handles_.reset();
    return;
  }

  if (!scoped_selection_handles_) {
    scoped_selection_handles_ =
        std::make_unique<ui::WindowAndroid::ScopedSelectionHandles>(
            view_.GetWindowAndroid());
  }
}

void RenderWidgetHostViewAndroid::SetWebContentsAccessibility(
    WebContentsAccessibilityAndroid* web_contents_accessibility) {
  web_contents_accessibility_ = web_contents_accessibility;
  UpdateReportAllRootScrolls();
}

void RenderWidgetHostViewAndroid::SetNeedsBeginFrameForFlingProgress() {
  if (sync_compositor_)
    sync_compositor_->RequestOneBeginFrame();
}

void RenderWidgetHostViewAndroid::BeginRotationBatching() {
  in_rotation_ = true;
  rotation_metrics_.emplace_back(
      std::make_pair(base::TimeTicks::Now(), viz::LocalSurfaceId()));
  // When a rotation begins, a series of calls update different aspects of
  // visual properties. Completing in EndRotationBatching, where the full new
  // set of properties is known. Trace the duration of that.
  const auto delta = rotation_metrics_.back().first - base::TimeTicks();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "viz", "RenderWidgetHostViewAndroid::RotationBegin",
      TRACE_ID_LOCAL(delta.InNanoseconds()), "visible", is_showing_);
}

void RenderWidgetHostViewAndroid::EndRotationBatching() {
  in_rotation_ = false;
  DCHECK(!rotation_metrics_.empty());
  const auto delta = rotation_metrics_.back().first - base::TimeTicks();
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "viz", "RenderWidgetHostViewAndroid::RotationBegin",
      TRACE_ID_LOCAL(delta.InNanoseconds()), "local_surface_id",
      local_surface_id_allocator_.GetCurrentLocalSurfaceId().ToString());
}

void RenderWidgetHostViewAndroid::BeginRotationEmbed() {
  DCHECK(!rotation_metrics_.empty());
  rotation_metrics_.back().second =
      local_surface_id_allocator_.GetCurrentLocalSurfaceId();

  // The full set of visual properties for a rotation is now known. This
  // tracks the time it takes until the Renderer successfully submits a frame
  // embedding the new viz::LocalSurfaceId. Tracking how long until a user
  // sees the complete rotation and layout of the page. This completes in
  // OnRenderFrameMetadataChangedAfterActivation.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "viz", "RenderWidgetHostViewAndroid::RotationEmbed",
      TRACE_ID_LOCAL(
          local_surface_id_allocator_.GetCurrentLocalSurfaceId().hash()),
      base::TimeTicks::Now(), "LocalSurfaceId",
      local_surface_id_allocator_.GetCurrentLocalSurfaceId().ToString());
}

}  // namespace content
