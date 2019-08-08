// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace viz {

class TextureDeleter;

class FakeSkiaOutputSurface : public SkiaOutputSurface {
 public:
  static std::unique_ptr<FakeSkiaOutputSurface> Create3d() {
    auto provider = TestContextProvider::Create();
    provider->BindToCurrentThread();
    return base::WrapUnique(new FakeSkiaOutputSurface(std::move(provider)));
  }

  static std::unique_ptr<FakeSkiaOutputSurface> Create3d(
      scoped_refptr<ContextProvider> provider) {
    return base::WrapUnique(new FakeSkiaOutputSurface(std::move(provider)));
  }

  ~FakeSkiaOutputSurface() override;

  // OutputSurface implementation:
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  std::unique_ptr<OverlayCandidateValidator> TakeOverlayCandidateValidator()
      override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  unsigned UpdateGpuFence() override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ResourceMetadata>& metadatas,
      SkYUVColorSpace yuv_color_space,
      sk_sp<SkColorSpace> dst_color_space,
      bool has_alpha) override;
  void SkiaSwapBuffers(OutputSurfaceFrame frame) override;
  SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap,
                                 sk_sp<SkColorSpace> color_space) override;
  gpu::SyncToken SubmitPaint(base::OnceClosure on_finished) override;
  sk_sp<SkImage> MakePromiseSkImage(const ResourceMetadata& metadata) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const RenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;

  void RemoveRenderPassResource(std::vector<RenderPassId> ids) override;
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;

  // ExternalUseClient implementation:
  void ReleaseCachedResources(const std::vector<ResourceId>& ids) override;

  // If set true, callbacks triggering will be in a reverse order as SignalQuery
  // calls.
  void SetOutOfOrderCallbacks(bool out_of_order_callbacks);

 private:
  explicit FakeSkiaOutputSurface(
      scoped_refptr<ContextProvider> context_provider);

  ContextProvider* context_provider() { return context_provider_.get(); }
  GrContext* gr_context() { return context_provider()->GrContext(); }

  bool GetGrBackendTexture(const ResourceMetadata& metadata,
                           GrBackendTexture* backend_texture);
  void SwapBuffersAck();

  scoped_refptr<ContextProvider> context_provider_;
  OutputSurfaceClient* client_ = nullptr;

  std::unique_ptr<TextureDeleter> texture_deleter_;

  // The current render pass id set by BeginPaintRenderPass.
  RenderPassId current_render_pass_id_ = 0;

  // SkSurfaces for render passes, sk_surfaces_[0] is the root surface.
  base::flat_map<RenderPassId, sk_sp<SkSurface>> sk_surfaces_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FakeSkiaOutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeSkiaOutputSurface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_
