// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_VULKAN_OVERLAY_RENDERER_H_
#define UI_OZONE_DEMO_VULKAN_OVERLAY_RENDERER_H_

#include <vulkan/vulkan.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace gpu {
class VulkanDeviceQueue;
class VulkanImplementation;
class VulkanCommandBuffer;
class VulkanCommandPool;
}  // namespace gpu

namespace ui {
class OverlaySurface;
class SurfaceFactoryOzone;

class VulkanOverlayRenderer : public RendererBase {
 public:
  VulkanOverlayRenderer(std::unique_ptr<OverlaySurface> overlay_surface,
                        SurfaceFactoryOzone* surface_factory_ozone,
                        gpu::VulkanImplementation* vulkan_instance,
                        gfx::AcceleratedWidget widget,
                        const gfx::Size& size);
  ~VulkanOverlayRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  void DestroyBuffers();
  void RecreateBuffers();
  void RenderFrame();
  void PostRenderFrameTask();
  void OnFrameSubmitted(uint64_t frame_sequence, gfx::SwapResult swap_result);
  void OnFramePresented(uint64_t frame_sequence,
                        const gfx::PresentationFeedback& feedback);
  void OnFrameReleased(uint64_t frame_sequence);

  class Buffer {
   public:
    Buffer(const gfx::Size& size,
           scoped_refptr<gfx::NativePixmap> native_pixmap,
           VkDeviceMemory vk_device_memory,
           VkImage vk_image,
           VkImageView vk_image_view,
           VkFramebuffer vk_framebuffer);
    ~Buffer();

    static std::unique_ptr<Buffer> Create(
        SurfaceFactoryOzone* surface_factory_ozone,
        VkDevice vk_device,
        VkRenderPass vk_render_pass,
        gfx::AcceleratedWidget widget,
        const gfx::Size& size);

    const gfx::Size& size() const { return size_; }
    const scoped_refptr<gfx::NativePixmap>& native_pixmap() const {
      return native_pixmap_;
    }
    VkDeviceMemory vk_device_memory() const { return vk_device_memory_; }
    VkImage vk_image() const { return vk_image_; }
    VkImageView vk_image_view() const { return vk_image_view_; }
    VkFramebuffer vk_framebuffer() const { return vk_framebuffer_; }

   private:
    const scoped_refptr<gfx::NativePixmap> native_pixmap_;
    const gfx::Size size_;
    const VkDeviceMemory vk_device_memory_;
    const VkImage vk_image_;
    const VkImageView vk_image_view_;
    const VkFramebuffer vk_framebuffer_;
  };

  uint64_t frame_sequence_ = 0;
  int next_buffer_ = 0;
  size_t in_use_buffers_ = 0;
  std::unique_ptr<Buffer> buffers_[2];

  SurfaceFactoryOzone* const surface_factory_ozone_;
  gpu::VulkanImplementation* const vulkan_implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
  std::unique_ptr<gpu::VulkanCommandPool> command_pool_;
  std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer_;
  std::unique_ptr<OverlaySurface> overlay_surface_;

  VkRenderPass render_pass_;

  base::WeakPtrFactory<VulkanOverlayRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VulkanOverlayRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_VULKAN_OVERLAY_RENDERER_H_
