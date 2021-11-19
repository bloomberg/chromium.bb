/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_fe_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

namespace {

using TaskRunnerHandle = scheduler::WebResourceLoadingTaskRunnerHandle;

class FailingLoader final : public WebURLLoader {
 public:
  explicit FailingLoader(
      std::unique_ptr<TaskRunnerHandle> freezable_task_runner_handle,
      std::unique_ptr<TaskRunnerHandle> unfreezable_task_runner_handle)
      : freezable_task_runner_handle_(std::move(freezable_task_runner_handle)),
        unfreezable_task_runner_handle_(
            std::move(unfreezable_task_runner_handle)) {}
  ~FailingLoader() override = default;

  // WebURLLoader implementation:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      absl::optional<WebURLError>& error,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    NOTREACHED();
  }
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override {
    NOTREACHED();
  }
  void Freeze(LoaderFreezeMode) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {}
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return freezable_task_runner_handle_->GetTaskRunner();
  }

 private:
  const std::unique_ptr<TaskRunnerHandle> freezable_task_runner_handle_;
  const std::unique_ptr<TaskRunnerHandle> unfreezable_task_runner_handle_;
};

class FailingLoaderFactory final : public WebURLLoaderFactory {
 public:
  // WebURLLoaderFactory implementation:
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<TaskRunnerHandle> freezable_task_runner_handle,
      std::unique_ptr<TaskRunnerHandle> unfreezable_task_runner_handle,
      CrossVariantMojoRemote<blink::mojom::KeepAliveHandleInterfaceBase>
          keep_alive_handle,
      WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper)
      override {
    return std::make_unique<FailingLoader>(
        std::move(freezable_task_runner_handle),
        std::move(unfreezable_task_runner_handle));
  }
};

}  // namespace

// SVGImageLocalFrameClient is used to wait until SVG document's load event
// in the case where there are subresources asynchronously loaded.
//
// Reference cycle: SVGImage -(Persistent)-> Page -(Member)-> Frame -(Member)->
// FrameClient == SVGImageLocalFrameClient -(raw)-> SVGImage.
class SVGImage::SVGImageLocalFrameClient : public EmptyLocalFrameClient {
 public:
  SVGImageLocalFrameClient(SVGImage* image) : image_(image) {}

  void ClearImage() { image_ = nullptr; }

 private:
  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    // SVG Images have unique security rules that prevent all subresource
    // requests except for data urls.
    return std::make_unique<FailingLoaderFactory>();
  }

  void DispatchDidHandleOnloadEvents() override {
    // The SVGImage was destructed before SVG load completion.
    if (!image_)
      return;

    image_->LoadCompleted();
  }

  // Cleared manually by SVGImage's destructor when |image_| is destructed.
  SVGImage* image_;
};

SVGImage::SVGImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      paint_controller_(std::make_unique<PaintController>()),
      // TODO(chikamune): use an existing AgentGroupScheduler
      // SVG will be shared via MemoryCache (which is renderer process
      // global cache) across multiple AgentSchedulingGroups. That's
      // why we can't use an existing AgentSchedulingGroup for now. If
      // we incorrectly use the existing ASG/AGS and if we freeze task
      // queues on a AGS, it will affect SVGs on other AGS. To
      // mitigate this problem, we need to split the MemoryCache into
      // smaller granularity. There is an active effort to mitigate
      // this which is called "Memory Cache Per Context"
      // (https://crbug.com/1127971).
      agent_group_scheduler_(
          Thread::MainThread()->Scheduler()->CreateAgentGroupScheduler()),
      has_pending_timeline_rewind_(false) {}

SVGImage::~SVGImage() {
  AllowDestroyingLayoutObjectInFinalizerScope scope;

  if (frame_client_)
    frame_client_->ClearImage();

  if (page_) {
    // It is safe to allow UA events within this scope, because event
    // dispatching inside the SVG image's document doesn't trigger JavaScript
    // execution. All script execution is forbidden when an SVG is loaded as an
    // image subresource - see SetScriptEnabled in SVGImage::DataChanged().
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
    // Store m_page in a local variable, clearing m_page, so that
    // SVGImageChromeClient knows we're destructed.
    Page* current_page = page_.Release();
    // Break both the loader and view references to the frame
    current_page->WillBeDestroyed();
  }

  // Verify that page teardown destroyed the Chrome
  DCHECK(!chrome_client_ || !chrome_client_->GetImage());
}

bool SVGImage::IsInSVGImage(const Node* node) {
  DCHECK(node);

  Page* page = node->GetDocument().GetPage();
  if (!page)
    return false;

  return page->GetChromeClient().IsSVGImageChromeClient();
}

LocalFrame* SVGImage::GetFrame() const {
  DCHECK(page_);
  return To<LocalFrame>(page_->MainFrame());
}

SVGSVGElement* SVGImage::RootElement() const {
  if (!page_)
    return nullptr;
  return DynamicTo<SVGSVGElement>(GetFrame()->GetDocument()->documentElement());
}

LayoutSVGRoot* SVGImage::LayoutRoot() const {
  if (SVGSVGElement* root_element = RootElement())
    return To<LayoutSVGRoot>(root_element->GetLayoutObject());
  return nullptr;
}

void SVGImage::CheckLoaded() const {
  CHECK(page_);
  // Failures of this assertion might result in wrong origin tainting checks,
  // because CurrentFrameHasSingleSecurityOrigin() assumes all subresources of
  // the SVG are loaded and thus ready for origin checks.
  CHECK(GetFrame()->GetDocument()->LoadEventFinished());
}

bool SVGImage::CurrentFrameHasSingleSecurityOrigin() const {
  if (!page_)
    return true;

  CheckLoaded();

  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return true;

  // Don't allow foreignObject elements or images that are not known to be
  // single-origin since these can leak cross-origin information.
  for (Node* node = root_element; node; node = FlatTreeTraversal::Next(*node)) {
    if (IsA<SVGForeignObjectElement>(*node))
      return false;
    if (auto* image = DynamicTo<SVGImageElement>(*node)) {
      if (!image->CurrentFrameHasSingleSecurityOrigin())
        return false;
    } else if (auto* fe_image = DynamicTo<SVGFEImageElement>(*node)) {
      if (!fe_image->CurrentFrameHasSingleSecurityOrigin())
        return false;
    }
  }

  // Because SVG image rendering disallows external resources and links, these
  // images effectively are restricted to a single security origin.
  return true;
}

IntSize SVGImage::SizeWithConfig(SizeConfig) const {
  return RoundedIntSize(intrinsic_size_);
}

static float ResolveWidthForRatio(float height,
                                  const FloatSize& intrinsic_ratio) {
  return height * intrinsic_ratio.width() / intrinsic_ratio.height();
}

static float ResolveHeightForRatio(float width,
                                   const FloatSize& intrinsic_ratio) {
  return width * intrinsic_ratio.height() / intrinsic_ratio.width();
}

bool SVGImage::HasIntrinsicSizingInfo() const {
  return LayoutRoot();
}

bool SVGImage::GetIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  const LayoutSVGRoot* layout_root = LayoutRoot();
  if (!layout_root)
    return false;
  layout_root->UnscaledIntrinsicSizingInfo(intrinsic_sizing_info);
  return true;
}

FloatSize SVGImage::ConcreteObjectSize(
    const FloatSize& default_object_size) const {
  IntrinsicSizingInfo intrinsic_sizing_info;
  if (!GetIntrinsicSizingInfo(intrinsic_sizing_info))
    return FloatSize();

  // https://www.w3.org/TR/css3-images/#default-sizing
  if (intrinsic_sizing_info.has_width && intrinsic_sizing_info.has_height)
    return intrinsic_sizing_info.size;

  // We're not using an intrinsic aspect ratio to resolve a missing
  // intrinsic width or height when preserveAspectRatio is none.
  // (Ref: crbug.com/584172)
  SVGSVGElement* svg = RootElement();
  if (svg->preserveAspectRatio()->CurrentValue()->Align() ==
      SVGPreserveAspectRatio::kSvgPreserveaspectratioNone)
    return default_object_size;

  if (intrinsic_sizing_info.has_width) {
    if (intrinsic_sizing_info.aspect_ratio.IsEmpty()) {
      return FloatSize(intrinsic_sizing_info.size.width(),
                       default_object_size.height());
    }
    return FloatSize(intrinsic_sizing_info.size.width(),
                     ResolveHeightForRatio(intrinsic_sizing_info.size.width(),
                                           intrinsic_sizing_info.aspect_ratio));
  }

  if (intrinsic_sizing_info.has_height) {
    if (intrinsic_sizing_info.aspect_ratio.IsEmpty()) {
      return FloatSize(default_object_size.width(),
                       intrinsic_sizing_info.size.height());
    }
    return FloatSize(ResolveWidthForRatio(intrinsic_sizing_info.size.height(),
                                          intrinsic_sizing_info.aspect_ratio),
                     intrinsic_sizing_info.size.height());
  }

  if (!intrinsic_sizing_info.aspect_ratio.IsEmpty()) {
    // "A contain constraint is resolved by setting the concrete object size to
    //  the largest rectangle that has the object's intrinsic aspect ratio and
    //  additionally has neither width nor height larger than the constraint
    //  rectangle's width and height, respectively."
    float solution_width = ResolveWidthForRatio(
        default_object_size.height(), intrinsic_sizing_info.aspect_ratio);
    if (solution_width <= default_object_size.width())
      return FloatSize(solution_width, default_object_size.height());

    float solution_height = ResolveHeightForRatio(
        default_object_size.width(), intrinsic_sizing_info.aspect_ratio);
    return FloatSize(default_object_size.width(), solution_height);
  }

  return default_object_size;
}

SVGImage::DrawInfo::DrawInfo(const FloatSize& container_size,
                             float zoom,
                             const KURL& url,
                             bool is_dark_mode_enabled)
    : container_size_(container_size),
      rounded_container_size_(RoundedIntSize(container_size)),
      zoom_(zoom),
      url_(url),
      is_dark_mode_enabled_(is_dark_mode_enabled) {}

FloatSize SVGImage::DrawInfo::CalculateResidualScale() const {
  return FloatSize(rounded_container_size_.width() / container_size_.width(),
                   rounded_container_size_.height() / container_size_.height());
}

void SVGImage::DrawForContainer(const DrawInfo& draw_info,
                                cc::PaintCanvas* canvas,
                                const PaintFlags& flags,
                                const FloatRect& dst_rect,
                                const FloatRect& src_rect) {
  FloatRect unzoomed_src = src_rect;
  unzoomed_src.Scale(1 / draw_info.Zoom());

  // Compensate for the container size rounding by adjusting the source rect.
  FloatSize residual_scale = draw_info.CalculateResidualScale();
  unzoomed_src.set_size(unzoomed_src.size().ScaledBy(residual_scale.width(),
                                                     residual_scale.height()));

  DrawInternal(draw_info, canvas, flags, dst_rect, unzoomed_src);
}

PaintImage SVGImage::PaintImageForCurrentFrame() {
  const DrawInfo draw_info(FloatSize(intrinsic_size_), 1, NullURL(), false);
  auto builder = CreatePaintImageBuilder();
  PopulatePaintRecordForCurrentFrameForContainer(draw_info, builder);
  return builder.TakePaintImage();
}

void SVGImage::DrawPatternForContainer(const DrawInfo& draw_info,
                                       GraphicsContext& context,
                                       const cc::PaintFlags& base_flags,
                                       const FloatRect& dst_rect,
                                       const ImageTilingInfo& tiling_info) {
  // Tile adjusted for scaling/stretch.
  FloatRect tile(tiling_info.image_rect);
  tile.Scale(tiling_info.scale.width(), tiling_info.scale.height());

  // Expand the tile to account for repeat spacing.
  FloatRect spaced_tile(tile);
  spaced_tile.Expand(tiling_info.spacing);

  SkMatrix pattern_transform;
  pattern_transform.setTranslate(tiling_info.phase.x() + spaced_tile.x(),
                                 tiling_info.phase.y() + spaced_tile.y());

  auto* builder = MakeGarbageCollected<PaintRecordBuilder>(context);
  {
    DrawingRecorder recorder(builder->Context(), *builder,
                             DisplayItem::Type::kSVGImage);
    // When generating an expanded tile, make sure we don't draw into the
    // spacing area.
    if (!tiling_info.spacing.IsZero())
      builder->Context().Clip(tile);
    DrawForContainer(draw_info, builder->Context().Canvas(), PaintFlags(), tile,
                     tiling_info.image_rect);
  }

  sk_sp<PaintShader> tile_shader = PaintShader::MakePaintRecord(
      builder->EndRecording(), spaced_tile, SkTileMode::kRepeat,
      SkTileMode::kRepeat, &pattern_transform);

  // If the shader could not be instantiated (e.g. non-invertible matrix),
  // draw transparent.
  // Note: we can't simply bail, because of arbitrary blend mode.
  PaintFlags flags = base_flags;
  flags.setColor(tile_shader ? SK_ColorBLACK : SK_ColorTRANSPARENT);
  flags.setShader(std::move(tile_shader));
  // Reset filter quality.
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kNone);

  context.DrawRect(dst_rect, flags,
                   PaintAutoDarkMode(DarkModeFilter::ElementRole::kSVG,
                                     draw_info.IsDarkModeEnabled()));

  StartAnimation();
}

void SVGImage::PopulatePaintRecordForCurrentFrameForContainer(
    const DrawInfo& draw_info,
    PaintImageBuilder& builder) {
  PaintRecorder recorder;
  const FloatSize size(draw_info.ContainerSize().ScaledBy(draw_info.Zoom()));
  const IntRect dest_rect(gfx::Point(), RoundedIntSize(size));
  cc::PaintCanvas* canvas = recorder.beginRecording(dest_rect);
  DrawForContainer(draw_info, canvas, PaintFlags(), FloatRect(dest_rect),
                   FloatRect(FloatPoint(), size));
  builder.set_paint_record(recorder.finishRecordingAsPicture(),
                           ToGfxRect(dest_rect),
                           PaintImage::GetNextContentId());

  builder.set_completion_state(
      load_state_ == LoadState::kLoadCompleted
          ? PaintImage::CompletionState::DONE
          : PaintImage::CompletionState::PARTIALLY_DONE);
}

bool SVGImage::ApplyShaderInternal(const DrawInfo& draw_info,
                                   PaintFlags& flags,
                                   const SkMatrix& local_matrix) {
  if (draw_info.ContainerSize().IsEmpty())
    return false;
  sk_sp<PaintRecord> record = PaintRecordForCurrentFrame(draw_info);
  if (!record)
    return false;

  const FloatRect bounds(FloatPoint(), draw_info.ContainerSize());
  flags.setShader(PaintShader::MakePaintRecord(
      std::move(record), bounds, SkTileMode::kClamp, SkTileMode::kClamp,
      &local_matrix));

  // Animation is normally refreshed in Draw() impls, which we don't reach when
  // painting via shaders.
  StartAnimation();
  return true;
}

bool SVGImage::ApplyShader(PaintFlags& flags,
                           const SkMatrix& local_matrix,
                           const FloatRect& dst_rect,
                           const FloatRect& src_rect,
                           const ImageDrawOptions& draw_options) {
  const DrawInfo draw_info(FloatSize(intrinsic_size_), 1, NullURL(),
                           draw_options.apply_dark_mode);
  return ApplyShaderInternal(draw_info, flags, local_matrix);
}

bool SVGImage::ApplyShaderForContainer(const DrawInfo& draw_info,
                                       PaintFlags& flags,
                                       const SkMatrix& local_matrix) {
  // Compensate for the container size rounding.
  FloatSize residual_scale =
      draw_info.CalculateResidualScale().ScaledBy(draw_info.Zoom());
  auto adjusted_local_matrix = local_matrix;
  adjusted_local_matrix.preScale(residual_scale.width(),
                                 residual_scale.height());
  return ApplyShaderInternal(draw_info, flags, adjusted_local_matrix);
}

void SVGImage::Draw(cc::PaintCanvas* canvas,
                    const PaintFlags& flags,
                    const FloatRect& dst_rect,
                    const FloatRect& src_rect,
                    const ImageDrawOptions& draw_options) {
  const DrawInfo draw_info(FloatSize(intrinsic_size_), 1, NullURL(),
                           draw_options.apply_dark_mode);
  DrawInternal(draw_info, canvas, flags, dst_rect, src_rect);
}

sk_sp<PaintRecord> SVGImage::PaintRecordForCurrentFrame(
    const DrawInfo& draw_info) {
  if (!page_)
    return nullptr;
  // Temporarily disable the image observer to prevent ChangeInRect() calls due
  // re-laying out the image.
  ImageObserverDisabler disable_image_observer(this);

  if (LayoutSVGRoot* layout_root = LayoutRoot())
    layout_root->SetContainerSize(RoundedLayoutSize(draw_info.ContainerSize()));
  LocalFrameView* view = GetFrame()->View();
  const IntSize rounded_container_size = draw_info.RoundedContainerSize();
  view->Resize(rounded_container_size);
  page_->GetVisualViewport().SetSize(rounded_container_size);

  // Always call processUrlFragment, even if the url is empty, because
  // there may have been a previous url/fragment that needs to be reset.
  view->ProcessUrlFragment(draw_info.Url(), /*same_document_navigation=*/false);

  // If the image was reset, we need to rewind the timeline back to 0. This
  // needs to be done before painting, or else we wouldn't get the correct
  // reset semantics (we'd paint the "last" frame rather than the one at
  // time=0.) The reason we do this here and not in resetAnimation() is to
  // avoid setting timers from the latter.
  FlushPendingTimelineRewind();

  page_->GetSettings().SetForceDarkModeEnabled(draw_info.IsDarkModeEnabled());

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    view->UpdateAllLifecyclePhases(DocumentUpdateReason::kSVGImage);
    return view->GetPaintRecord();
  }

  // TODO(crbug.com/1203406): This works around the bug. We may want to find
  // and fix the root cause, or do nothing until pre-CAP code is removed.
  if (!view->GetLayoutView() || !view->GetLayoutView()->Compositor())
    return nullptr;

  view->UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kSVGImage);
  PaintController::CycleScope cycle_scope(*paint_controller_,
                                          view->PaintDebugInfoEnabled());

  auto* builder = MakeGarbageCollected<PaintRecordBuilder>(*paint_controller_);
  view->PaintOutsideOfLifecycle(builder->Context(), kGlobalPaintNormalPhase);
  return builder->EndRecording();
}

static bool DrawNeedsLayer(const PaintFlags& flags) {
  if (SkColorGetA(flags.getColor()) < 255)
    return true;

  // This is needed to preserve the dark mode filter that
  // has been set in GraphicsContext.
  if (flags.getColorFilter())
    return true;

  return flags.getBlendMode() != SkBlendMode::kSrcOver;
}

void SVGImage::DrawInternal(const DrawInfo& draw_info,
                            cc::PaintCanvas* canvas,
                            const PaintFlags& flags,
                            const FloatRect& dst_rect,
                            const FloatRect& unzoomed_src_rect) {
  sk_sp<PaintRecord> record = PaintRecordForCurrentFrame(draw_info);
  if (!record)
    return;

  {
    PaintCanvasAutoRestore ar(canvas, false);
    if (DrawNeedsLayer(flags)) {
      SkRect layer_rect = dst_rect;
      canvas->saveLayer(&layer_rect, &flags);
    }
    // We can only draw the entire frame, clipped to the rect we want. So
    // compute where the top left of the image would be if we were drawing
    // without clipping, and translate accordingly.
    canvas->save();
    canvas->clipRect(EnclosingIntRect(dst_rect));
    canvas->concat(SkMatrix::RectToRect(unzoomed_src_rect, dst_rect));
    canvas->drawPicture(std::move(record));
    canvas->restore();
  }

  // Start any (SMIL) animations if needed. This will restart or continue
  // animations if preceded by calls to resetAnimation or stopAnimation
  // respectively.
  StartAnimation();
}

void SVGImage::ScheduleTimelineRewind() {
  has_pending_timeline_rewind_ = true;
}

void SVGImage::FlushPendingTimelineRewind() {
  if (!has_pending_timeline_rewind_)
    return;
  if (SVGSVGElement* root_element = RootElement())
    root_element->setCurrentTime(0);
  has_pending_timeline_rewind_ = false;
}

void SVGImage::StartAnimation() {
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return;
  chrome_client_->ResumeAnimation();
  if (root_element->animationsPaused())
    root_element->unpauseAnimations();
}

void SVGImage::StopAnimation() {
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return;
  chrome_client_->SuspendAnimation();
  root_element->pauseAnimations();
}

void SVGImage::ResetAnimation() {
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return;
  chrome_client_->SuspendAnimation();
  root_element->pauseAnimations();
  ScheduleTimelineRewind();
}

void SVGImage::RestoreAnimation() {
  // If the image has no animations then do nothing.
  if (!MaybeAnimated())
    return;
  // If there are no clients, or no client is going to render, then do nothing.
  ImageObserver* image_observer = GetImageObserver();
  if (!image_observer || image_observer->ShouldPauseAnimation(this))
    return;
  StartAnimation();
}

bool SVGImage::MaybeAnimated() {
  SVGSVGElement* root_element = RootElement();
  if (!root_element)
    return false;
  return root_element->TimeContainer()->HasAnimations() ||
         root_element->GetDocument().Timeline().HasPendingUpdates();
}

void SVGImage::ServiceAnimations(
    base::TimeTicks monotonic_animation_start_time) {
  if (!GetImageObserver())
    return;

  // If none of our observers (sic!) are visible, or for some other reason
  // does not want us to keep running animations, stop them until further
  // notice (next paint.)
  if (GetImageObserver()->ShouldPauseAnimation(this)) {
    StopAnimation();
    return;
  }

  // serviceScriptedAnimations runs requestAnimationFrame callbacks, but SVG
  // images can't have any so we assert there's no script.
  ScriptForbiddenScope forbid_script;

  // The calls below may trigger GCs, so set up the required persistent
  // reference on the ImageResourceContent which owns this SVGImage. By
  // transitivity, that will keep the associated SVGImageChromeClient object
  // alive.
  Persistent<ImageObserver> protect(GetImageObserver());
  page_->Animator().ServiceScriptedAnimations(monotonic_animation_start_time);

  // Do *not* update the paint phase. It's critical to paint only when
  // actually generating painted output, not only for performance reasons,
  // but to preserve correct coherence of the cache of the output with
  // the needsRepaint bits of the PaintLayers in the image.
  LocalFrame* frame = GetFrame();
  LocalFrameView* frame_view = frame->View();
  frame_view->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kSVGImage);

  // We run UpdateAnimations after the paint phase, but per the above comment,
  // we don't want to run lifecycle through to paint for SVG images. Since we
  // know SVG images never have composited animations, we can update animations
  // directly without worrying about including PaintArtifactCompositor's
  // analysis of whether animations should be composited.
  frame->GetDocument()->GetDocumentAnimations().UpdateAnimations(
      DocumentLifecycle::kLayoutClean, nullptr, false);
}

void SVGImage::AdvanceAnimationForTesting() {
  if (SVGSVGElement* root_element = RootElement()) {
    root_element->TimeContainer()->AdvanceFrameForTesting();

    // The following triggers animation updates which can issue a new draw
    // and temporarily change the animation timeline. It's necessary to call
    // reset before changing to a time value as animation clock does not
    // expect to go backwards.
    base::TimeTicks current_animation_time =
        page_->Animator().Clock().CurrentTime();
    page_->Animator().Clock().ResetTimeForTesting();
    if (root_element->TimeContainer()->IsStarted())
      root_element->TimeContainer()->ResetDocumentTime();
    page_->Animator().ServiceScriptedAnimations(
        root_element->GetDocument().Timeline().CalculateZeroTime() +
        base::Seconds(root_element->getCurrentTime()));
    GetImageObserver()->Changed(this);
    page_->Animator().Clock().ResetTimeForTesting();
    page_->Animator().Clock().UpdateTime(current_animation_time);
  }
}

SVGImageChromeClient& SVGImage::ChromeClientForTesting() {
  return *chrome_client_;
}

void SVGImage::UpdateUseCounters(const Document& document) const {
  if (SVGSVGElement* root_element = RootElement()) {
    if (root_element->TimeContainer()->HasAnimations()) {
      document.CountUse(WebFeature::kSVGSMILAnimationInImageRegardlessOfCache);
    }
  }
}

void SVGImage::LoadCompleted() {
  switch (load_state_) {
    case kInDataChanged:
      load_state_ = kLoadCompleted;
      break;

    case kWaitingForAsyncLoadCompletion:
      load_state_ = kLoadCompleted;

      // Because LoadCompleted() is called synchronously from
      // Document::ImplicitClose(), we defer AsyncLoadCompleted() to avoid
      // potential bugs and timing dependencies around ImplicitClose() and
      // to make LoadEventFinished() true when AsyncLoadCompleted() is called.
      GetFrame()
          ->GetTaskRunner(TaskType::kInternalLoading)
          ->PostTask(FROM_HERE, WTF::Bind(&SVGImage::NotifyAsyncLoadCompleted,
                                          scoped_refptr<SVGImage>(this)));
      break;

    case kDataChangedNotStarted:
    case kLoadCompleted:
      CHECK(false);
      break;
  }
}

void SVGImage::NotifyAsyncLoadCompleted() {
  if (GetImageObserver())
    GetImageObserver()->AsyncLoadCompleted(this);
}

Image::SizeAvailability SVGImage::DataChanged(bool all_data_received) {
  TRACE_EVENT0("blink", "SVGImage::dataChanged");

  // Don't do anything if is an empty image.
  if (!DataSize())
    return kSizeAvailable;

  if (!all_data_received)
    return page_ ? kSizeAvailable : kSizeUnavailable;

  CHECK(!page_);

  // SVGImage will fire events (and the default C++ handlers run) but doesn't
  // actually allow script to run so it's fine to call into it. We allow this
  // since it means an SVG data url can synchronously load like other image
  // types.
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_user_agent_events;

  CHECK_EQ(load_state_, kDataChangedNotStarted);
  load_state_ = kInDataChanged;

  chrome_client_ = MakeGarbageCollected<SVGImageChromeClient>(this);

  // FIXME: If this SVG ends up loading itself, we might leak the world.
  // The Cache code does not know about ImageResources holding Frames and
  // won't know to break the cycle.
  // This will become an issue when SVGImage will be able to load other
  // SVGImage objects, but we're safe now, because SVGImage can only be
  // loaded by a top-level document.
  Page* page;
  {
    TRACE_EVENT0("blink", "SVGImage::dataChanged::createPage");
    page = Page::CreateNonOrdinary(*chrome_client_, *agent_group_scheduler_);
    page->GetSettings().SetScriptEnabled(false);
    page->GetSettings().SetPluginsEnabled(false);

    // Because this page is detached, it can't get default font settings
    // from the embedder. Copy over font settings so we have sensible
    // defaults. These settings are fixed and will not update if changed.
    if (!Page::OrdinaryPages().IsEmpty()) {
      Settings& default_settings =
          (*Page::OrdinaryPages().begin())->GetSettings();
      page->GetSettings().GetGenericFontFamilySettings() =
          default_settings.GetGenericFontFamilySettings();
      page->GetSettings().SetMinimumFontSize(
          default_settings.GetMinimumFontSize());
      page->GetSettings().SetMinimumLogicalFontSize(
          default_settings.GetMinimumLogicalFontSize());
      page->GetSettings().SetDefaultFontSize(
          default_settings.GetDefaultFontSize());
      page->GetSettings().SetDefaultFixedFontSize(
          default_settings.GetDefaultFixedFontSize());

      page->GetSettings().SetImageAnimationPolicy(
          default_settings.GetImageAnimationPolicy());

      // Also copy the preferred-color-scheme to ensure a responsiveness to
      // dark/light color schemes.
      page->GetSettings().SetPreferredColorScheme(
          default_settings.GetPreferredColorScheme());
    }
    chrome_client_->InitAnimationTimer(page->GetPageScheduler()
                                           ->GetAgentGroupScheduler()
                                           .CompositorTaskRunner());
  }

  LocalFrame* frame = nullptr;
  {
    TRACE_EVENT0("blink", "SVGImage::dataChanged::createFrame");
    DCHECK(!frame_client_);
    frame_client_ = MakeGarbageCollected<SVGImageLocalFrameClient>(this);
    frame = MakeGarbageCollected<LocalFrame>(
        frame_client_, *page, nullptr, nullptr, nullptr,
        FrameInsertType::kInsertInConstructor, LocalFrameToken(), nullptr,
        nullptr);
    frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame));
    frame->Init(/*opener=*/nullptr, /*policy_container=*/nullptr);
  }

  // SVG Images will always synthesize a viewBox, if it's not available, and
  // thus never see scrollbars.
  frame->View()->SetCanHaveScrollbars(false);
  // SVG Images are transparent.
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);

  TRACE_EVENT0("blink", "SVGImage::dataChanged::load");

  frame->ForceSynchronousDocumentInstall("image/svg+xml", Data());

  // Set up our Page reference after installing our document. This avoids
  // tripping on a non-existing (null) Document if a GC is triggered during the
  // set up and ends up collecting the last owner/observer of this image.
  page_ = page;

  // Intrinsic sizing relies on computed style (e.g. font-size and
  // writing-mode).
  frame->GetDocument()->UpdateStyleAndLayoutTree();

  DCHECK(page_);
  switch (load_state_) {
    case kInDataChanged:
      load_state_ = kWaitingForAsyncLoadCompletion;
      break;
    case kLoadCompleted:
      break;
    case kDataChangedNotStarted:
    case kWaitingForAsyncLoadCompletion:
      CHECK(false);
      break;
  }

  if (!RootElement())
    return kSizeUnavailable;

  // Set the concrete object size before a container size is available.
  intrinsic_size_ = RoundedLayoutSize(ConcreteObjectSize(FloatSize(
      LayoutReplaced::kDefaultWidth, LayoutReplaced::kDefaultHeight)));

  if (load_state_ == kWaitingForAsyncLoadCompletion)
    return kSizeAvailableAndLoadingAsynchronously;
  DCHECK_EQ(load_state_, kLoadCompleted);
  return kSizeAvailable;
}

bool SVGImage::IsSizeAvailable() {
  return RootElement();
}

String SVGImage::FilenameExtension() const {
  return "svg";
}

}  // namespace blink
