// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/vulkan_renderer.h"

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
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

namespace ui {

VulkanRenderer::VulkanRenderer(std::unique_ptr<gpu::VulkanSurface> surface,
                               gpu::VulkanImplementation* vulkan_implementation,
                               gfx::AcceleratedWidget widget,
                               const gfx::Size& size)
    : RendererBase(widget, size),
      vulkan_implementation_(vulkan_implementation),
      surface_(std::move(surface)),
      size_(size),
      weak_ptr_factory_(this) {}

VulkanRenderer::~VulkanRenderer() {
  DestroyFramebuffers();
  DestroyRenderPass();
  surface_->Destroy();
  surface_.reset();
  command_pool_->Destroy();
  command_pool_.reset();
  device_queue_->Destroy();
  device_queue_.reset();
}

bool VulkanRenderer::Initialize() {
  TRACE_EVENT1("ozone", "VulkanRenderer::Initialize", "widget", widget_);

  device_queue_ = gpu::CreateVulkanDeviceQueue(
      vulkan_implementation_,
      gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
          gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  if (!device_queue_) {
    LOG(FATAL) << "Failed to init device queue";
  }

  if (!surface_->Initialize(device_queue_.get(),
                            gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
    LOG(FATAL) << "Failed to init surface";
  }

  VkAttachmentDescription render_pass_attachments[] = {{
      /* .flags = */ 0,
      /* .format = */ surface_->surface_format().format,
      /* .samples = */ VK_SAMPLE_COUNT_1_BIT,
      /* .loadOp = */ VK_ATTACHMENT_LOAD_OP_CLEAR,
      /* .storeOp = */ VK_ATTACHMENT_STORE_OP_STORE,
      /* .stencilLoadOp = */ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      /* .stencilStoreOp = */ VK_ATTACHMENT_STORE_OP_DONT_CARE,
      /* .initialLayout = */ VK_IMAGE_LAYOUT_UNDEFINED,
      /* .finalLayout = */ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  }};

  VkAttachmentReference color_attachment_references[] = {
      {/* .attachment = */ 0,
       /* .layout = */ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription render_pass_subpasses[] = {{
      /* .flags = */ 0,
      /* .pipelineBindPoint = */ VK_PIPELINE_BIND_POINT_GRAPHICS,
      /* .inputAttachmentCount = */ 0,
      /* .pInputAttachments = */ nullptr,
      /* .colorAttachmentCount = */ base::size(color_attachment_references),
      /* .pColorAttachments = */ color_attachment_references,
      /* .pResolveAttachments = */ nullptr,
      /* .pDepthStencilAttachment = */ nullptr,
      /* .preserveAttachmentCount = */ 0,
      /* .pPreserveAttachments = */ nullptr,
  }};

  VkRenderPassCreateInfo render_pass_create_info = {
      /* .sType = */ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      /* .pNext = */ nullptr,
      /* .flags = */ 0,
      /* .attachmentCount = */ base::size(render_pass_attachments),
      /* .pAttachments = */ render_pass_attachments,
      /* .subpassCount = */ base::size(render_pass_subpasses),
      /* .pSubpasses = */ render_pass_subpasses,
      /* .dependencyCount = */ 0,
      /* .pDependencies = */ nullptr,
  };

  CHECK_EQ(vkCreateRenderPass(device_queue_->GetVulkanDevice(),
                              &render_pass_create_info, nullptr, &render_pass_),
           VK_SUCCESS);

  command_pool_ = std::make_unique<gpu::VulkanCommandPool>(device_queue_.get());
  CHECK(command_pool_->Initialize());

  RecreateFramebuffers();

  // Schedule the initial render.
  PostRenderFrameTask();
  return true;
}

void VulkanRenderer::DestroyRenderPass() {
  if (render_pass_ == VK_NULL_HANDLE)
    return;

  vkDestroyRenderPass(device_queue_->GetVulkanDevice(), render_pass_, nullptr);
  render_pass_ = VK_NULL_HANDLE;
}

void VulkanRenderer::DestroyFramebuffers() {
  VkDevice vk_device = device_queue_->GetVulkanDevice();

  VkResult result = vkQueueWaitIdle(device_queue_->GetVulkanQueue());
  CHECK_EQ(result, VK_SUCCESS);

  for (std::unique_ptr<Framebuffer>& framebuffer : framebuffers_) {
    if (!framebuffer)
      continue;

    framebuffer->command_buffer()->Destroy();
    vkDestroyFramebuffer(vk_device, framebuffer->vk_framebuffer(), nullptr);
    vkDestroyImageView(vk_device, framebuffer->vk_image_view(), nullptr);
    framebuffer.reset();
  }
}

void VulkanRenderer::RecreateFramebuffers() {
  TRACE_EVENT0("ozone", "VulkanRenderer::RecreateFramebuffers");

  DestroyFramebuffers();

  surface_->SetSize(size_);

  gpu::VulkanSwapChain* vulkan_swap_chain = surface_->GetSwapChain();
  const uint32_t num_images = vulkan_swap_chain->num_images();
  framebuffers_.resize(num_images);

  for (uint32_t image = 0; image < num_images; ++image) {
    framebuffers_[image] =
        Framebuffer::Create(device_queue_.get(), command_pool_.get(),
                            render_pass_, surface_.get(), image);
    CHECK(framebuffers_[image]);
  }
}

void VulkanRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "VulkanRenderer::RenderFrame");

  VkClearValue clear_value = {
      /* .color = */ {/* .float32 = */ {.5f, 1.f - NextFraction(), .5f, 1.f}}};

  gpu::VulkanSwapChain* vulkan_swap_chain = surface_->GetSwapChain();
  const uint32_t image = vulkan_swap_chain->current_image();
  const Framebuffer& framebuffer = *framebuffers_[image];

  gpu::VulkanCommandBuffer& command_buffer = *framebuffer.command_buffer();

  {
    gpu::ScopedSingleUseCommandBufferRecorder recorder(command_buffer);

    VkRenderPassBeginInfo begin_info = {
        /* .sType = */ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        /* .pNext = */ nullptr,
        /* .renderPass = */ render_pass_,
        /* .framebuffer = */ framebuffer.vk_framebuffer(),
        /* .renderArea = */
        {
            /* .offset = */ {
                /* .x = */ 0,
                /* .y = */ 0,
            },
            /* .extent = */
            {
                /* .width = */ vulkan_swap_chain->size().width(),
                /* .height = */ vulkan_swap_chain->size().height(),
            },
        },
        /* .clearValueCount = */ 1,
        /* .pClearValues = */ &clear_value,
    };

    vkCmdBeginRenderPass(recorder.handle(), &begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdEndRenderPass(recorder.handle());
  }

  CHECK(command_buffer.Submit(0, nullptr, 0, nullptr));

  vulkan_swap_chain->SwapBuffers();

  PostRenderFrameTask();
}

void VulkanRenderer::PostRenderFrameTask() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VulkanRenderer::RenderFrame,
                                weak_ptr_factory_.GetWeakPtr()));
}

VulkanRenderer::Framebuffer::Framebuffer(
    VkImageView vk_image_view,
    VkFramebuffer vk_framebuffer,
    std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer)
    : vk_image_view_(vk_image_view),
      vk_framebuffer_(vk_framebuffer),
      command_buffer_(std::move(command_buffer)) {}

VulkanRenderer::Framebuffer::~Framebuffer() {}

std::unique_ptr<VulkanRenderer::Framebuffer>
VulkanRenderer::Framebuffer::Create(gpu::VulkanDeviceQueue* vulkan_device_queue,
                                    gpu::VulkanCommandPool* vulkan_command_pool,
                                    VkRenderPass vk_render_pass,
                                    gpu::VulkanSurface* vulkan_surface,
                                    uint32_t vulkan_swap_chain_image_index) {
  gpu::VulkanSwapChain* vulkan_swap_chain = vulkan_surface->GetSwapChain();
  const VkDevice vk_device = vulkan_device_queue->GetVulkanDevice();
  VkImageViewCreateInfo vk_image_view_create_info = {
      /* .sType = */ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /* .pNext = */ nullptr,
      /* .flags = */ 0,
      /* .image = */ vulkan_swap_chain->GetImage(vulkan_swap_chain_image_index),
      /* .viewType = */ VK_IMAGE_VIEW_TYPE_2D,
      /* .format = */ vulkan_surface->surface_format().format,
      /* .components = */
      {
          /* .r = */ VK_COMPONENT_SWIZZLE_IDENTITY,
          /* .b = */ VK_COMPONENT_SWIZZLE_IDENTITY,
          /* .g = */ VK_COMPONENT_SWIZZLE_IDENTITY,
          /* .a = */ VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      /* .subresourceRange = */
      {
          /* .aspectMask = */ VK_IMAGE_ASPECT_COLOR_BIT,
          /* .baseMipLevel = */ 0,
          /* .levelCount = */ 1,
          /* .baseArrayLayer = */ 0,
          /* .layerCount = */ 1,
      },
  };

  VkResult result;
  VkImageView vk_image_view = VK_NULL_HANDLE;
  result = vkCreateImageView(vk_device, &vk_image_view_create_info, nullptr,
                             &vk_image_view);
  if (result != VK_SUCCESS) {
    LOG(FATAL) << "Failed to create a Vulkan image view.";
  }
  VkFramebufferCreateInfo vk_framebuffer_create_info = {
      /* .sType = */ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      /* .pNext = */ nullptr,
      /* .flags = */ 0,
      /* .renderPass = */ vk_render_pass,
      /* .attachmentCount = */ 1,
      /* .pAttachments = */ &vk_image_view,
      /* .width = */ vulkan_swap_chain->size().width(),
      /* .height = */ vulkan_swap_chain->size().height(),
      /* .layers = */ 1,
  };

  VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;
  result = vkCreateFramebuffer(vk_device, &vk_framebuffer_create_info, nullptr,
                               &vk_framebuffer);
  if (result != VK_SUCCESS) {
    LOG(FATAL) << "Failed to create a Vulkan framebuffer.";
  }

  auto command_buffer = std::make_unique<gpu::VulkanCommandBuffer>(
      vulkan_device_queue, vulkan_command_pool, true /* primary */);
  CHECK(command_buffer->Initialize());

  return std::make_unique<VulkanRenderer::Framebuffer>(
      vk_image_view, vk_framebuffer, std::move(command_buffer));
}

}  // namespace ui
