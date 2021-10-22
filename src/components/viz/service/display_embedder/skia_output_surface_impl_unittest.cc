// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
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
  DebugRendererSettings debug_settings_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  std::unique_ptr<DisplayCompositorMemoryAndTaskController> display_controller_;
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
  auto skia_deps = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
      GetGpuService(), gpu::kNullSurfaceHandle);
  display_controller_ =
      std::make_unique<DisplayCompositorMemoryAndTaskController>(
          std::move(skia_deps));
  output_surface_ = SkiaOutputSurfaceImpl::Create(display_controller_.get(),
                                                  settings, &debug_settings_);
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
  output_surface_->EndPaint(std::move(closure));
  return output_surface_->Flush();
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
  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  auto result_bitmap = scoped_bitmap.bitmap();
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

TEST_F(SkiaOutputSurfaceImplTest, EndPaint) {
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
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_rect, color_space));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  output_surface_->CopyOutput(AggregatedRenderPassId{0}, geometry, color_space,
                              std::move(request), gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  BlockMainThread();

  // EndPaint draw is deferred until CopyOutput.
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
    output_surface_->Flush();

    run_loop.Run();
  }
}

// Tests that the destination color space is preserved across a CopyOutput for
// ColorSpaces supported by SkColorSpace.
TEST_F(SkiaOutputSurfaceImplTest, CopyOutputBitmapSupportedColorSpace) {
  output_surface_->Reshape(kSurfaceRect.size(), 1, gfx::ColorSpace(),
                           gfx::BufferFormat::RGBX_8888, /*use_stencil=*/false);

  constexpr gfx::Rect output_rect(0, 0, 10, 10);
  const gfx::ColorSpace color_space = gfx::ColorSpace(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::LINEAR);
  base::RunLoop run_loop;
  std::unique_ptr<CopyOutputResult> result;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](std::unique_ptr<CopyOutputResult>* result_out,
             base::OnceClosure quit_closure,
             std::unique_ptr<CopyOutputResult> tmp_result) {
            *result_out = std::move(tmp_result);
            std::move(quit_closure).Run();
          },
          &result, run_loop.QuitClosure()));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  PaintRootRenderPass(kSurfaceRect, base::DoNothing());
  output_surface_->CopyOutput(AggregatedRenderPassId{0}, geometry, color_space,
                              std::move(request), gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  run_loop.Run();

  EXPECT_EQ(color_space, result->GetRGBAColorSpace());
}

// Tests that copying from a source with a color space that can't be converted
// to a SkColorSpace will fallback to a transform to sRGB.
TEST_F(SkiaOutputSurfaceImplTest, CopyOutputBitmapUnsupportedColorSpace) {
  output_surface_->Reshape(kSurfaceRect.size(), 1, gfx::ColorSpace(),
                           gfx::BufferFormat::RGBX_8888, /*use_stencil=*/false);

  constexpr gfx::Rect output_rect(0, 0, 10, 10);
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreatePiecewiseHDR(
      gfx::ColorSpace::PrimaryID::BT2020, 0.5, 1.5);
  base::RunLoop run_loop;
  std::unique_ptr<CopyOutputResult> result;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](std::unique_ptr<CopyOutputResult>* result_out,
             base::OnceClosure quit_closure,
             std::unique_ptr<CopyOutputResult> tmp_result) {
            *result_out = std::move(tmp_result);
            std::move(quit_closure).Run();
          },
          &result, run_loop.QuitClosure()));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  PaintRootRenderPass(kSurfaceRect, base::DoNothing());
  output_surface_->CopyOutput(AggregatedRenderPassId{0}, geometry, color_space,
                              std::move(request), gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  run_loop.Run();

  EXPECT_EQ(gfx::ColorSpace::CreateSRGB(), result->GetRGBAColorSpace());
}

}  // namespace viz
