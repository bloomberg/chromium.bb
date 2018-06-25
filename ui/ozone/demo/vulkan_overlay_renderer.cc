// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/vulkan_overlay_renderer.h"

#include <vulkan/vulkan.h>

#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_render_pass.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/overlay_surface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

namespace {

// If we only submit one buffer, it'll never be released back to us for reuse.
constexpr int kMinimumBuffersForForwardProgress = 2;

}  // namespace

VulkanOverlayRenderer::VulkanOverlayRenderer(
    std::unique_ptr<OverlaySurface> overlay_surface,
    SurfaceFactoryOzone* surface_factory_ozone,
    gpu::VulkanImplementation* vulkan_implementation,
    gfx::AcceleratedWidget widget,
    const gfx::Size& size)
    : RendererBase(widget, size),
      surface_factory_ozone_(surface_factory_ozone),
      vulkan_implementation_(vulkan_implementation),
      vulkan_function_pointers_(gpu::GetVulkanFunctionPointers()),
      overlay_surface_(std::move(overlay_surface)),
      weak_ptr_factory_(this) {}

VulkanOverlayRenderer::~VulkanOverlayRenderer() {
  for (auto& buffer : buffers_)
    buffer.reset();
  command_buffer_->Destroy();
  command_buffer_.reset();
  command_pool_->Destroy();
  command_pool_.reset();
  device_queue_->Destroy();
  device_queue_.reset();
}

bool VulkanOverlayRenderer::Initialize() {
  TRACE_EVENT1("ozone", "VulkanOverlayRenderer::Initialize", "widget", widget_);

  device_queue_ = gpu::CreateVulkanDeviceQueue(
      vulkan_implementation_, gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG);
  if (!device_queue_) {
    LOG(FATAL) << "Failed to init device queue";
  }

  VkAttachmentDescription render_pass_attachments[] = {{
      .format = VK_FORMAT_B8G8R8A8_SRGB,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  }};

  VkAttachmentReference color_attachment_references[] = {
      {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription render_pass_subpasses[] = {{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .colorAttachmentCount = base::size(color_attachment_references),
      .pColorAttachments = color_attachment_references,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr,
  }};

  VkRenderPassCreateInfo render_pass_create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = base::size(render_pass_attachments),
      .pAttachments = render_pass_attachments,
      .subpassCount = base::size(render_pass_subpasses),
      .pSubpasses = render_pass_subpasses,
      .dependencyCount = 0,
  };

  CHECK_EQ(vulkan_function_pointers_->vkCreateRenderPass(
               device_queue_->GetVulkanDevice(), &render_pass_create_info,
               nullptr, &render_pass_),
           VK_SUCCESS);

  command_pool_ = std::make_unique<gpu::VulkanCommandPool>(device_queue_.get());
  CHECK(command_pool_->Initialize());

  command_buffer_ = std::make_unique<gpu::VulkanCommandBuffer>(
      device_queue_.get(), command_pool_.get(), true /* primary */);
  CHECK(command_buffer_->Initialize());

  RecreateBuffers();

  // Schedule the initial render.
  PostRenderFrameTask();
  return true;
}

void VulkanOverlayRenderer::RecreateBuffers() {
  TRACE_EVENT0("ozone", "VulkanOverlayRenderer::RecreateBuffers");

  for (auto& buffer : buffers_) {
    buffer = Buffer::Create(surface_factory_ozone_, vulkan_function_pointers_,
                            device_queue_->GetVulkanDevice(), render_pass_,
                            widget_, size_);
    CHECK(buffer);
  }
}

void VulkanOverlayRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "VulkanOverlayRenderer::RenderFrame");

  VkClearValue clear_value = {
      .color = {.float32 = {.5f, 1.f - NextFraction(), .5f, 1.f}}};

  const Buffer& buffer = *buffers_[next_buffer_];
  next_buffer_++;
  next_buffer_ %= base::size(buffers_);
  ++in_use_buffers_;
  DCHECK_LE(in_use_buffers_, base::size(buffers_));

  {
    gpu::ScopedSingleUseCommandBufferRecorder recorder(*command_buffer_);

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = render_pass_;
    begin_info.framebuffer = buffer.vk_framebuffer();
    begin_info.renderArea.extent.width = buffer.size().width();
    begin_info.renderArea.extent.height = buffer.size().height();
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;

    vulkan_function_pointers_->vkCmdBeginRenderPass(
        recorder.handle(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vulkan_function_pointers_->vkCmdEndRenderPass(recorder.handle());
  }

  VkSemaphoreCreateInfo sem_create_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VkSemaphore semaphore;
  vulkan_function_pointers_->vkCreateSemaphore(
      device_queue_->GetVulkanDevice(), &sem_create_info, NULL, &semaphore);

  command_buffer_->Submit(1, &semaphore, 0, nullptr);

  // TODO(spang): Create a gfx::GpuFence and use it for synchronization.
  command_buffer_->Wait(UINT64_MAX);

  ui::OverlayPlane primary_plane;
  primary_plane.pixmap = buffer.native_pixmap();
  primary_plane.display_bounds = gfx::Rect(buffer.size());
  primary_plane.crop_rect = gfx::RectF(1.f, 1.f);

  std::vector<ui::OverlayPlane> overlay_planes;
  overlay_planes.push_back(std::move(primary_plane));

  uint64_t frame_sequence = ++frame_sequence_;

  overlay_surface_->SubmitFrame(
      std::move(overlay_planes),
      base::BindOnce(&VulkanOverlayRenderer::OnFrameSubmitted,
                     weak_ptr_factory_.GetWeakPtr(), frame_sequence),
      base::BindOnce(&VulkanOverlayRenderer::OnFramePresented,
                     weak_ptr_factory_.GetWeakPtr(), frame_sequence),
      base::BindOnce(&VulkanOverlayRenderer::OnFrameReleased,
                     weak_ptr_factory_.GetWeakPtr(), frame_sequence));
}

void VulkanOverlayRenderer::PostRenderFrameTask() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VulkanOverlayRenderer::RenderFrame,
                                weak_ptr_factory_.GetWeakPtr()));
}

void VulkanOverlayRenderer::OnFrameSubmitted(uint64_t frame_sequence,
                                             gfx::SwapResult swap_result) {
  TRACE_EVENT1("ozone", "VulkanOverlayRenderer::OnFrameSubmitted", "frame",
               frame_sequence);

  CHECK_NE(swap_result, gfx::SwapResult::SWAP_FAILED);

  if (swap_result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS)
    RecreateBuffers();

  if (in_use_buffers_ < kMinimumBuffersForForwardProgress)
    PostRenderFrameTask();
}

void VulkanOverlayRenderer::OnFramePresented(
    uint64_t frame_sequence,
    const gfx::PresentationFeedback& feedback) {
  TRACE_EVENT1("ozone", "VulkanOverlayRenderer::OnFramePresented", "frame",
               frame_sequence);
}

void VulkanOverlayRenderer::OnFrameReleased(uint64_t frame_sequence) {
  TRACE_EVENT1("ozone", "VulkanOverlayRenderer::OnFrameReleased", "frame",
               frame_sequence);

  --in_use_buffers_;

  PostRenderFrameTask();
}

VulkanOverlayRenderer::Buffer::Buffer(
    const gfx::Size& size,
    scoped_refptr<gfx::NativePixmap> native_pixmap,
    VkDeviceMemory vk_device_memory,
    VkImage vk_image,
    VkImageView vk_image_view,
    VkFramebuffer vk_framebuffer)
    : native_pixmap_(std::move(native_pixmap)),
      size_(size),
      vk_device_memory_(vk_device_memory),
      vk_image_(vk_image),
      vk_image_view_(vk_image_view),
      vk_framebuffer_(vk_framebuffer) {}

VulkanOverlayRenderer::Buffer::~Buffer() {}

std::unique_ptr<VulkanOverlayRenderer::Buffer>
VulkanOverlayRenderer::Buffer::Create(
    SurfaceFactoryOzone* surface_factory_ozone,
    const gpu::VulkanFunctionPointers* vulkan_function_pointers,
    VkDevice vk_device,
    VkRenderPass vk_render_pass,
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  gfx::BufferFormat format = gfx::BufferFormat::BGRA_8888;

  VkImage vk_image = VK_NULL_HANDLE;
  VkDeviceMemory vk_device_memory = VK_NULL_HANDLE;
  scoped_refptr<gfx::NativePixmap> native_pixmap =
      surface_factory_ozone->CreateNativePixmapForVulkan(
          widget, size, format, gfx::BufferUsage::SCANOUT,
          vulkan_function_pointers, vk_device, &vk_device_memory, &vk_image);
  if (!native_pixmap) {
    LOG(FATAL)
        << "Failed to create a presentable buffer for rendering with vulkan";
  }

  VkImageViewCreateInfo vk_image_view_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vk_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_SRGB,
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  VkResult result;
  VkImageView vk_image_view = VK_NULL_HANDLE;
  result = vulkan_function_pointers->vkCreateImageView(
      vk_device, &vk_image_view_create_info, nullptr, &vk_image_view);
  if (result != VK_SUCCESS) {
    LOG(FATAL) << "Failed to create a Vulkan image view.";
  }
  VkFramebufferCreateInfo vk_framebuffer_create_info{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = vk_render_pass,
      .attachmentCount = 1,
      .pAttachments = &vk_image_view,
      .width = size.width(),
      .height = size.height(),
      .layers = 1,
  };

  VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;
  result = vulkan_function_pointers->vkCreateFramebuffer(
      vk_device, &vk_framebuffer_create_info, nullptr, &vk_framebuffer);
  if (result != VK_SUCCESS) {
    LOG(FATAL) << "Failed to create a Vulkan framebuffer.";
  }

  return std::make_unique<VulkanOverlayRenderer::Buffer>(
      size, native_pixmap, vk_device_memory, vk_image, vk_image_view,
      vk_framebuffer);
}

}  // namespace ui
