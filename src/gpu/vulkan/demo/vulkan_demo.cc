// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/demo/vulkan_demo.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/platform_window/x11/x11_window.h"

namespace gpu {

VulkanDemo::VulkanDemo() = default;

VulkanDemo::~VulkanDemo() = default;

void VulkanDemo::Initialize() {
  vulkan_implementation_ = gpu::CreateVulkanImplementation();
  DCHECK(vulkan_implementation_) << ":Failed to create vulkan implementation.";

  auto result = vulkan_implementation_->InitializeVulkanInstance();
  DCHECK(result) << "Failed to initialize vulkan implementation.";

  vulkan_context_provider_ =
      viz::VulkanInProcessContextProvider::Create(vulkan_implementation_.get());
  DCHECK(vulkan_context_provider_)
      << "Failed to create vulkan context provider.";

  event_source_ = ui::PlatformEventSource::CreateDefault();

  gfx::Size size(800, 600);
  window_ = std::make_unique<ui::X11Window>(
      this, gfx::Rect(gfx::Point(100, 100), size));
  window_->Show();

  // Sync up size between |window_| and |vulkan_surface_|
  vulkan_surface_->SetSize(size);
  sk_surfaces_.resize(vulkan_surface_->GetSwapChain()->num_images());
}

void VulkanDemo::Destroy() {
  vulkan_surface_->Destroy();
}

void VulkanDemo::Run() {
  DCHECK(!is_running_);
  DCHECK(!run_loop_);
  base::RunLoop run_loop;
  is_running_ = true;
  run_loop_ = &run_loop;
  RenderFrame();
  run_loop.Run();
  run_loop_ = nullptr;
}

void VulkanDemo::OnBoundsChanged(const gfx::Rect& new_bounds) {
  if (vulkan_surface_->size() == new_bounds.size())
    return;
  auto old_size = vulkan_surface_->size();
  vulkan_surface_->SetSize(new_bounds.size());
  if (vulkan_surface_->size() != old_size) {
    // Size has been changed, we need to clear all surfaces which will be
    // recreated later.
    sk_surfaces_.clear();
    sk_surfaces_.resize(vulkan_surface_->GetSwapChain()->num_images());
  }
}

void VulkanDemo::OnCloseRequest() {
  is_running_ = false;
  if (run_loop_)
    run_loop_->QuitWhenIdle();
}

void VulkanDemo::OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) {
  DCHECK_EQ(accelerated_widget_, gfx::kNullAcceleratedWidget);
  accelerated_widget_ = widget;

  vulkan_surface_ =
      vulkan_implementation_->CreateViewSurface(accelerated_widget_);
  DCHECK(vulkan_surface_);

  auto result =
      vulkan_surface_->Initialize(vulkan_context_provider_->GetDeviceQueue(),
                                  gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT);
  DCHECK(result) << "Failed to initialize vulkan surface.";
}

void VulkanDemo::CreateSkSurface() {
  auto* swap_chain = vulkan_surface_->GetSwapChain();
  auto index = swap_chain->current_image();
  auto& sk_surface = sk_surfaces_[index];

  if (!sk_surface) {
    SkSurfaceProps surface_props =
        SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);
    VkImage vk_image = swap_chain->GetCurrentImage();
    VkImageLayout vk_image_layout = swap_chain->GetCurrentImageLayout();
    GrVkImageInfo vk_image_info;
    vk_image_info.fImage = vk_image;
    vk_image_info.fAlloc = {VK_NULL_HANDLE, 0, 0, 0};
    vk_image_info.fImageLayout = vk_image_layout;
    vk_image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    vk_image_info.fFormat = VK_FORMAT_B8G8R8A8_UNORM;
    vk_image_info.fLevelCount = 1;
    const auto& size = vulkan_surface_->size();
    GrBackendRenderTarget render_target(size.width(), size.height(), 0, 0,
                                        vk_image_info);
    sk_surface = SkSurface::MakeFromBackendRenderTarget(
        vulkan_context_provider_->GetGrContext(), render_target,
        kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr,
        &surface_props);
  } else {
    auto backend = sk_surface->getBackendRenderTarget(
        SkSurface::kFlushRead_BackendHandleAccess);
    backend.setVkImageLayout(swap_chain->GetCurrentImageLayout());
  }
  sk_surface_ = sk_surface;
}

void VulkanDemo::Draw(SkCanvas* canvas, float fraction) {
  canvas->save();
  canvas->clear(SkColorSetARGB(255, 255 * fraction, 255 * (1 - fraction), 0));

  constexpr float kWidth = 800;
  constexpr float kHeight = 600;

  const auto& size = vulkan_surface_->size();
  canvas->scale(size.width() / kWidth, size.height() / kHeight);

  SkPaint paint;
  paint.setColor(SK_ColorRED);

  // Draw a rectangle with red paint
  SkRect rect = SkRect::MakeXYWH(10, 10, 128, 128);
  canvas->drawRect(rect, paint);

  // Set up a linear gradient and draw a circle
  {
    SkPoint linearPoints[] = {{0, 0}, {300, 300}};
    SkColor linearColors[] = {SK_ColorGREEN, SK_ColorBLACK};
    paint.setShader(SkGradientShader::MakeLinear(
        linearPoints, linearColors, nullptr, 2, SkShader::kMirror_TileMode));
    paint.setAntiAlias(true);

    canvas->drawCircle(200, 200, 64, paint);

    // Detach shader
    paint.setShader(nullptr);
  }

  // Draw a message with a nice black paint
  paint.setColor(SK_ColorBLACK);

  SkFont font;
  font.setSize(32);
  font.setSubpixel(true);

  static const char message[] = "Hello Vulkan";

  // Translate and rotate
  canvas->translate(300, 300);
  rotation_angle_ += 0.2f;
  if (rotation_angle_ > 360) {
    rotation_angle_ -= 360;
  }
  canvas->rotate(rotation_angle_);

  // Draw the text
  canvas->drawString(message, 0, 0, font, paint);

  canvas->restore();
  canvas->flush();
}

void VulkanDemo::RenderFrame() {
  if (!is_running_)
    return;
  CreateSkSurface();
  Draw(sk_surface_->getCanvas(), 0.7);
  auto backend = sk_surface_->getBackendRenderTarget(
      SkSurface::kFlushRead_BackendHandleAccess);
  GrVkImageInfo vk_image_info;
  if (!backend.getVkImageInfo(&vk_image_info))
    NOTREACHED() << "Failed to get image info";
  vulkan_surface_->GetSwapChain()->SetCurrentImageLayout(
      vk_image_info.fImageLayout);
  vulkan_surface_->SwapBuffers();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&VulkanDemo::RenderFrame, base::Unretained(this)));
}

}  // namespace gpu
