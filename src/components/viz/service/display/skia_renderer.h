// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cc/cc_export.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/sync_query_collection.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/latency/latency_info.h"

class SkColorFilter;
class SkRuntimeEffect;

namespace viz {
class AggregatedRenderPassDrawQuad;
class DebugBorderDrawQuad;
class DelegatedInkPointRendererBase;
class DelegatedInkHandler;
class PictureDrawQuad;
class SkiaOutputSurface;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;
class YUVVideoDrawQuad;

// TODO(795132): SkColorSpace is only a subset comparing to gfx::ColorSpace.
// Need to figure out support for color space that is not covered by
// SkColorSpace.
class VIZ_SERVICE_EXPORT SkiaRenderer : public DirectRenderer {
 public:
  // TODO(penghuang): Remove skia_output_surface when DDL is used everywhere.
  SkiaRenderer(const RendererSettings* settings,
               const DebugRendererSettings* debug_settings,
               OutputSurface* output_surface,
               DisplayResourceProviderSkia* resource_provider,
               OverlayProcessorInterface* overlay_processor,
               SkiaOutputSurface* skia_output_surface);

  SkiaRenderer(const SkiaRenderer&) = delete;
  SkiaRenderer& operator=(const SkiaRenderer&) = delete;

  ~SkiaRenderer() override;

  void SwapBuffers(SwapFrameData swap_frame_data) override;
  void SwapBuffersSkipped() override;
  void SwapBuffersComplete(gfx::GpuFenceHandle release_fence) override;
  void BuffersPresented() override;
  void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) override;

  void SetDisablePictureQuadImageFiltering(bool disable) {
    disable_picture_quad_image_filtering_ = disable;
  }

  DelegatedInkPointRendererBase* GetDelegatedInkPointRenderer(
      bool create_if_necessary) override;
  void SetDelegatedInkMetadata(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) override;

 protected:
  bool CanPartialSwap() override;
  void UpdateRenderPassTextures(
      const AggregatedRenderPassList& render_passes_in_draw_order,
      const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
          render_passes_in_frame) override;
  void AllocateRenderPassResourceIfNeeded(
      const AggregatedRenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) override;
  bool IsRenderPassResourceAllocated(
      const AggregatedRenderPassId& render_pass_id) const override;
  gfx::Size GetRenderPassBackingPixelSize(
      const AggregatedRenderPassId& render_pass_id) override;
  void BindFramebufferToOutputSurface() override;
  void BindFramebufferToTexture(
      const AggregatedRenderPassId render_pass_id) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  bool FlippedFramebuffer() const override;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(const copy_output::RenderPassGeometry& geometry,
                           std::unique_ptr<CopyOutputRequest> request) override;
  void DidChangeVisibility() override;
  void FinishDrawingQuadList() override;
  void GenerateMipmap() override;
  void SetDelegatedInkPointRendererSkiaForTest(
      std::unique_ptr<DelegatedInkPointRendererSkia> renderer) override;

  std::unique_ptr<DelegatedInkHandler> delegated_ink_handler_;

 private:
  enum class BypassMode;
  struct DrawQuadParams;
  struct DrawRPDQParams;
  class ScopedSkImageBuilder;
  class ScopedYUVSkImageBuilder;

  void ClearCanvas(SkColor color);
  void ClearFramebuffer();

  // Callers should init an SkAutoCanvasRestore before calling this function.
  // |scissor_rect| and |rounded_corner_bounds| should be in device space,
  // i.e. same space that |cdt| will transform subsequent draws into.
  void PrepareCanvas(const absl::optional<gfx::Rect>& scissor_rect,
                     const absl::optional<gfx::RRectF>& rounded_corner_bounds,
                     const gfx::Transform* cdt);
  // Further modify the canvas as needed to apply the effects represented by
  // |rpdq_params|. Call Prepare[Paint|Color]OrCanvasForRPDQ when possible,
  // in order apply the RPDQ effects into a more efficient format.
  void PrepareCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                            DrawQuadParams* params);
  // Attempt to apply the effects in |rpdq_params| to the paint used to draw
  // the quad; otherwise modify the current canvas instead.
  void PreparePaintOrCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                   DrawQuadParams* params,
                                   SkPaint* paint);
  // Attempt to apply the effects in |rpdq_params| to the color used to draw
  // the quad; otherwise modify the current canvas as a fallback.
  void PrepareColorOrCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                   DrawQuadParams* params,
                                   SkColor* color);

  // The returned DrawQuadParams can be modified by the DrawX calls that accept
  // params so that they can apply explicit data transforms before sending to
  // Skia in a consistent manner.
  DrawQuadParams CalculateDrawQuadParams(const gfx::Transform& target_to_device,
                                         const gfx::Rect* scissor_rect,
                                         const DrawQuad* quad,
                                         const gfx::QuadF* draw_region) const;

  DrawRPDQParams CalculateRPDQParams(const AggregatedRenderPassDrawQuad* quad,
                                     DrawQuadParams* params);
  // Modifies |params| and |rpdq_params| to apply correctly when drawing the
  // RenderPass directly via |bypass_quad|.
  BypassMode CalculateBypassParams(const DrawQuad* bypass_quad,
                                   DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) const;

  SkCanvas::ImageSetEntry MakeEntry(const SkImage* image,
                                    int matrix_index,
                                    const DrawQuadParams& params) const;
  // Returns overall constraint to pass to Skia, and modifies |params| to
  // emulate content area clamping different from the provided texture coords.
  SkCanvas::SrcRectConstraint ResolveTextureConstraints(
      const SkImage* image,
      const gfx::RectF& valid_texel_bounds,
      DrawQuadParams* params) const;

  bool MustFlushBatchedQuads(const DrawQuad* new_quad,
                             const DrawRPDQParams* rpdq_params,
                             const DrawQuadParams& params) const;
  void AddQuadToBatch(const SkImage* image,
                      const gfx::RectF& valid_texel_bounds,
                      DrawQuadParams* params);
  void FlushBatchedQuads();

  // Utility function that calls appropriate draw function based on quad
  // material. If |rpdq_params| is not null, then |quad| is assumed to be the
  // bypass quad associated with the RenderPass that defined the |rpdq_params|.
  void DrawQuadInternal(const DrawQuad* quad,
                        const DrawRPDQParams* rpdq_params,
                        DrawQuadParams* params);

  // Utility to draw a single quad as a filled color, and optionally apply the
  // effects defined in |rpdq_params| when the quad is bypassing the render pass
  void DrawColoredQuad(SkColor color,
                       const DrawRPDQParams* rpdq_params,
                       DrawQuadParams* params);
  // Utility to make a single ImageSetEntry and draw it with the complex paint,
  // and optionally apply the effects defined in |rpdq_params| when the quad is
  // bypassing the render pass
  void DrawSingleImage(const SkImage* image,
                       const gfx::RectF& valid_texel_bounds,
                       const DrawRPDQParams* rpdq_params,
                       SkPaint* paint,
                       DrawQuadParams* params);

  void DrawPaintOpBuffer(const cc::PaintOpBuffer* buffer,
                         const absl::optional<SkColor>& clear_color,
                         const TileDrawQuad* quad,
                         const DrawQuadParams* params);

  // RPDQ, DebugBorder and picture quads cannot be batched. They
  // either are not textures (debug, picture), or it's very likely
  // the texture will have advanced paint effects (rpdq). Additionally, they do
  // not support being drawn directly for a pass-through RenderPass.
  void DrawRenderPassQuad(const AggregatedRenderPassDrawQuad* quad,
                          DrawQuadParams* params);
  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                           DrawQuadParams* params);
  void DrawPictureQuad(const PictureDrawQuad* quad, DrawQuadParams* params);

  // Solid-color quads are not batchable, but can be drawn directly in place of
  // a RenderPass (hence it takes the optional DrawRPDQParams).
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                          const DrawRPDQParams* rpdq_params,
                          DrawQuadParams* params);

  void DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                           const DrawRPDQParams* rpdq_params,
                           DrawQuadParams* params);
  void DrawTextureQuad(const TextureDrawQuad* quad,
                       const DrawRPDQParams* rpdq_params,
                       DrawQuadParams* params);
  void DrawTileDrawQuad(const TileDrawQuad* quad,
                        const DrawRPDQParams* rpdq_params,
                        DrawQuadParams* params);
  void DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                        const DrawRPDQParams* rpdq_params,
                        DrawQuadParams* params);

  void DrawUnsupportedQuad(const DrawQuad* quad,
                           const DrawRPDQParams* rpdq_params,
                           DrawQuadParams* params);

  // Schedule overlay candidates for presentation at next SwapBuffers().
  void ScheduleOverlays();

  // skia_renderer can draw most single-quad passes directly, regardless of
  // blend mode or image filtering.
  const DrawQuad* CanPassBeDrawnDirectly(
      const AggregatedRenderPass* pass) override;

  void DrawDelegatedInkTrail() override;

  // Get a color filter that converts from |src| color space to |dst| color
  // space using a shader constructed from gfx::ColorTransform.  The color
  // filters are cached in |color_filter_cache_|.  Resource offset and
  // multiplier are used to adjust the RGB output of the shader for YUV video
  // quads. The default values perform no adjustment.
  sk_sp<SkColorFilter> GetColorSpaceConversionFilter(
      const gfx::ColorSpace& src,
      const gfx::ColorSpace& dst,
      float resource_offset = 0.0f,
      float resource_multiplier = 1.0f);
  // Returns the color filter that should be applied to the current canvas.
  sk_sp<SkColorFilter> GetContentColorFilter();

  // Flush SkiaOutputSurface, so all pending GPU tasks in SkiaOutputSurface will
  // be sent to GPU scheduler.
  void FlushOutputSurface();

#if defined(OS_APPLE) || defined(USE_OZONE)
  void PrepareRenderPassOverlay(
      OverlayProcessorInterface::PlatformOverlayCandidate* overlay);
#endif

  DisplayResourceProviderSkia* resource_provider() {
    return static_cast<DisplayResourceProviderSkia*>(resource_provider_);
  }

  // A map from RenderPass id to the texture used to draw the RenderPass from.
  struct RenderPassBacking {
    gfx::Size size;
    bool generate_mipmap;
    gfx::ColorSpace color_space;
    ResourceFormat format;
    gpu::Mailbox mailbox;
  };
  base::flat_map<AggregatedRenderPassId, RenderPassBacking>
      render_pass_backings_;

  // Interface used for drawing. Common among different draw modes.
  sk_sp<SkSurface> root_surface_;
  raw_ptr<SkCanvas> root_canvas_ = nullptr;
  raw_ptr<SkCanvas> current_canvas_ = nullptr;
  raw_ptr<SkSurface> current_surface_ = nullptr;
  class FrameResourceFence;
  scoped_refptr<FrameResourceFence> current_frame_resource_fence_;

  bool disable_picture_quad_image_filtering_ = false;
  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  gfx::Rect swap_buffer_rect_;

  // State common to all quads in a batch. Draws that require an SkPaint not
  // captured by this state cannot be batched.
  struct BatchedQuadState {
    absl::optional<gfx::Rect> scissor_rect;
    absl::optional<gfx::RRectF> rounded_corner_bounds;
    SkBlendMode blend_mode;
    SkSamplingOptions sampling;
    SkCanvas::SrcRectConstraint constraint;

    BatchedQuadState();
  };
  BatchedQuadState batched_quad_state_;
  std::vector<SkCanvas::ImageSetEntry> batched_quads_;
  // Same order as batched_quads_, but only includes draw regions for the
  // entries that have fHasClip == true. Each draw region is 4 consecutive pts
  std::vector<SkPoint> batched_draw_regions_;
  // Each entry of batched_quads_ will have an index into this vector; multiple
  // entries may point to the same matrix.
  std::vector<SkMatrix> batched_cdt_matrices_;

  // Specific for SkDDL.
  const raw_ptr<SkiaOutputSurface> skia_output_surface_;

  const bool is_using_raw_draw_;

  // Lock set for resources that are used for the current frame. All resources
  // in this set will be unlocked with a sync token when the frame is done in
  // the compositor thread. And the sync token will be released when the DDL
  // for the current frame is replayed on the GPU thread.
  // It is only used with DDL.
  absl::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;

  // Locks for overlays are pending for swapbuffers.
  base::circular_deque<
      std::vector<DisplayResourceProviderSkia::ScopedReadLockSharedImage>>
      pending_overlay_locks_;

  // Locks for overlays have been committed. |pending_overlay_locks_| will
  // be moved to |committed_overlay_locks_| after SwapBuffers() completed.
  std::vector<DisplayResourceProviderSkia::ScopedReadLockSharedImage>
      committed_overlay_locks_;

  // Locks for overlays that have release fences and read lock fences.
  base::circular_deque<
      std::vector<DisplayResourceProviderSkia::ScopedReadLockSharedImage>>
      read_lock_release_fence_overlay_locks_;

#if defined(OS_APPLE) || defined(USE_OZONE)
  class ScopedReadLockComparator {
   public:
    using is_transparent = void;
    bool operator()(
        const DisplayResourceProviderSkia::ScopedReadLockSharedImage& lhs,
        const DisplayResourceProviderSkia::ScopedReadLockSharedImage& rhs)
        const;
    bool operator()(
        const DisplayResourceProviderSkia::ScopedReadLockSharedImage& lhs,
        const gpu::Mailbox& rhs) const;
    bool operator()(
        const gpu::Mailbox& lhs,
        const DisplayResourceProviderSkia::ScopedReadLockSharedImage& rhs)
        const;
  };
  // a set for locks of overlays which are waiting for releasing.
  // The set is using lock.mailbox() as the unique key.
  base::flat_set<DisplayResourceProviderSkia::ScopedReadLockSharedImage,
                 ScopedReadLockComparator>
      awaiting_release_overlay_locks_;
#endif  // defined(OS_APPLE) || defined(USE_OZONE)

  base::flat_map<gfx::ColorSpace,
                 base::flat_map<gfx::ColorSpace, sk_sp<SkRuntimeEffect>>>
      color_filter_cache_;

  bool UsingSkiaForDelegatedInk() const;
  uint32_t debug_tint_modulate_count_ = 0;
  bool use_real_color_space_for_stream_video_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
