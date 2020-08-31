// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

namespace viz {
namespace {

constexpr gfx::Rect kSurfaceRect(0, 0, 100, 100);
constexpr SkColor kOutputColor = SK_ColorRED;

}  // namespace

class SkiaOutputSurfaceImplTest : public testing::Test {
 public:
  SkiaOutputSurfaceImplTest();
  ~SkiaOutputSurfaceImplTest() override;

  GpuServiceImpl* GetGpuService() {
    return TestGpuServiceHolder::GetInstance()->gpu_service();
  }

  void SetUpSkiaOutputSurfaceImpl();

  // Paints and submits root RenderPass with a solid color rect of |size|.
  gpu::SyncToken PaintRootRenderPass(const gfx::Rect& output_rect,
                                     base::OnceClosure closure);

  void CheckSyncTokenOnGpuThread(const gpu::SyncToken& sync_token);
  void CopyRequestCallbackOnGpuThread(const gfx::Rect& output_rect,
                                      const gfx::ColorSpace& color_space,
                                      std::unique_ptr<CopyOutputResult> result);
  void BlockMainThread();
  void UnblockMainThread();

 protected:
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  std::unique_ptr<SkiaOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  base::WaitableEvent wait_;
};

SkiaOutputSurfaceImplTest::SkiaOutputSurfaceImplTest()
    : wait_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
  SetUpSkiaOutputSurfaceImpl();
}

SkiaOutputSurfaceImplTest::~SkiaOutputSurfaceImplTest() {
  output_surface_.reset();
}

void SkiaOutputSurfaceImplTest::SetUpSkiaOutputSurfaceImpl() {
  RendererSettings settings;
  settings.use_skia_renderer = true;
  output_surface_ = SkiaOutputSurfaceImpl::Create(
      std::make_unique<SkiaOutputSurfaceDependencyImpl>(
          GetGpuService(), gpu::kNullSurfaceHandle),
      settings);
  output_surface_->BindToClient(&output_surface_client_);
}

gpu::SyncToken SkiaOutputSurfaceImplTest::PaintRootRenderPass(
    const gfx::Rect& rect,
    base::OnceClosure closure) {
  SkPaint paint;
  paint.setColor(kOutputColor);
  SkCanvas* root_canvas = output_surface_->BeginPaintCurrentFrame();
  root_canvas->drawRect(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.height(), rect.width()), paint);
  return output_surface_->SubmitPaint(std::move(closure));
}

void SkiaOutputSurfaceImplTest::BlockMainThread() {
  wait_.Wait();
}

void SkiaOutputSurfaceImplTest::UnblockMainThread() {
  DCHECK(!wait_.IsSignaled());
  wait_.Signal();
}

void SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread(
    const gpu::SyncToken& sync_token) {
  EXPECT_TRUE(
      GetGpuService()->sync_point_manager()->IsSyncTokenReleased(sync_token));
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread(
    const gfx::Rect& output_rect,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputResult> result) {
  SkBitmap result_bitmap(result->AsSkBitmap());
  EXPECT_EQ(result_bitmap.width(), output_rect.width());
  EXPECT_EQ(result_bitmap.height(), output_rect.height());

  SkBitmap expected;
  expected.allocPixels(SkImageInfo::MakeN32Premul(
      output_rect.width(), output_rect.height(), color_space.ToSkColorSpace()));
  expected.eraseColor(kOutputColor);

  EXPECT_TRUE(cc::MatchesBitmap(result_bitmap, expected,
                                cc::ExactPixelComparator(false)));

  UnblockMainThread();
}

TEST_F(SkiaOutputSurfaceImplTest, SubmitPaint) {
  output_surface_->Reshape(kSurfaceRect.size(), 1, gfx::ColorSpace(),
                           gfx::BufferFormat::RGBX_8888, /*use_stencil=*/false);
  constexpr gfx::Rect output_rect(0, 0, 10, 10);

  bool on_finished_called = false;
  base::OnceClosure on_finished =
      base::BindOnce([](bool* result) { *result = true; }, &on_finished_called);

  gpu::SyncToken sync_token =
      PaintRootRenderPass(output_rect, std::move(on_finished));
  EXPECT_TRUE(sync_token.HasData());

  // Copy the output
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_rect, color_space));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  output_surface_->CopyOutput(0, geometry, color_space, std::move(request));
  BlockMainThread();

  // SubmitPaint draw is deferred until CopyOutput.
  base::OnceClosure closure =
      base::BindOnce(&SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread,
                     base::Unretained(this), sync_token);

  output_surface_->ScheduleGpuTaskForTesting(std::move(closure), {sync_token});
  BlockMainThread();
  EXPECT_TRUE(on_finished_called);
}

// Draws two frames and calls Reshape() between the two frames changing the
// color space. Verifies draw after color space change is successful.
TEST_F(SkiaOutputSurfaceImplTest, SupportsColorSpaceChange) {
  for (auto& color_space : {gfx::ColorSpace(), gfx::ColorSpace::CreateSRGB()}) {
    output_surface_->Reshape(kSurfaceRect.size(), 1, color_space,
                             gfx::BufferFormat::RGBX_8888,
                             /*use_stencil=*/false);

    // Draw something, it's not important what.
    base::RunLoop run_loop;
    PaintRootRenderPass(kSurfaceRect, run_loop.QuitClosure());

    OutputSurfaceFrame frame;
    frame.size = kSurfaceRect.size();
    output_surface_->SwapBuffers(std::move(frame));

    run_loop.Run();
  }
}

}  // namespace viz
