// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_backing.h"

#include <utility>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_context.h"

#if defined(OS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(OS_LINUX)
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#endif

namespace gpu {

namespace {

VkResult CreateExternalVkImage(SharedContextState* context_state,
                               VkFormat format,
                               const gfx::Size& size,
                               bool is_transfer_dst,
                               VkImage* image) {
  VkExternalMemoryImageCreateInfoKHR external_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .handleTypes = context_state->vk_context_provider()
                         ->GetVulkanImplementation()
                         ->GetExternalImageHandleType(),
  };

  auto usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (is_transfer_dst)
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  VkImageCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = &external_info,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {size.width(), size.height(), 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkDevice device =
      context_state->vk_context_provider()->GetDeviceQueue()->GetVulkanDevice();
  return vkCreateImage(device, &create_info, nullptr, image);
}

void TransitionToColorAttachment(VkImage image,
                                 SharedContextState* context_state,
                                 VulkanCommandPool* command_pool) {
  auto command_buffer = command_pool->CreatePrimaryCommandBuffer();
  CHECK(command_buffer->Initialize());

  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    command_buffer->TransitionImageLayout(
        image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  // TODO(penghuang): get rid of this submission if poosible.
  command_buffer->Submit(0, nullptr, 0, nullptr);

  context_state->vk_context_provider()
      ->GetDeviceQueue()
      ->GetFenceHelper()
      ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(command_buffer));
}

uint32_t FindMemoryTypeIndex(SharedContextState* context_state,
                             const VkMemoryRequirements& requirements,
                             VkMemoryPropertyFlags flags) {
  VkPhysicalDevice physical_device = context_state->vk_context_provider()
                                         ->GetDeviceQueue()
                                         ->GetVulkanPhysicalDevice();
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
  constexpr uint32_t kInvalidTypeIndex = 32;
  for (uint32_t i = 0; i < kInvalidTypeIndex; i++) {
    if (((1u << i) & requirements.memoryTypeBits) == 0)
      continue;
    if ((properties.memoryTypes[i].propertyFlags & flags) != flags)
      continue;
    return i;
  }
  NOTREACHED();
  return kInvalidTypeIndex;
}

}  // namespace

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::Create(
    SharedContextState* context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data,
    bool using_gmb) {
  VkDevice device =
      context_state->vk_context_provider()->GetDeviceQueue()->GetVulkanDevice();
  VkFormat vk_format = ToVkFormat(format);
  VkImage image;
  bool is_transfer_dst = using_gmb || !pixel_data.empty();
  VkResult result = CreateExternalVkImage(context_state, vk_format, size,
                                          is_transfer_dst, &image);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create external VkImage: " << result;
    return nullptr;
  }

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device, image, &requirements);

  if (!requirements.memoryTypeBits) {
    DLOG(ERROR)
        << "Unable to find appropriate memory type for external VkImage";
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  VkExportMemoryAllocateInfoKHR external_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
      .handleTypes = context_state->vk_context_provider()
                         ->GetVulkanImplementation()
                         ->GetExternalImageHandleType(),
  };

  VkMemoryAllocateInfo mem_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &external_info,
      .allocationSize = requirements.size,
      .memoryTypeIndex = FindMemoryTypeIndex(
          context_state, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  VkDeviceMemory memory;
  // TODO(crbug.com/932286): Allocating a separate piece of memory for every
  // VkImage might have too much overhead. It is recommended that one large
  // VkDeviceMemory be sub-allocated to multiple VkImages instead.
  result = vkAllocateMemory(device, &mem_alloc_info, nullptr, &memory);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to allocate memory for external VkImage: " << result;
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  result = vkBindImageMemory(device, image, memory, 0);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind memory to external VkImage: " << result;
    vkFreeMemory(device, memory, nullptr);
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  // TODO(penghuang): track image layout to avoid unnecessary image layout
  // transition. https://crbug.com/965955
  TransitionToColorAttachment(image, context_state, command_pool);

  auto backing = base::WrapUnique(new ExternalVkImageBacking(
      mailbox, format, size, color_space, usage, context_state, image, memory,
      requirements.size, vk_format, command_pool));

  if (!pixel_data.empty())
    backing->WritePixels(pixel_data, 0);

  return backing;
}

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::CreateFromGMB(
    SharedContextState* context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (gfx::NumberOfPlanesForBufferFormat(buffer_format) != 1) {
    DLOG(ERROR) << "Invalid image format.";
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    DLOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  auto* vulkan_implementation =
      context_state->vk_context_provider()->GetVulkanImplementation();
  auto resource_format = viz::GetResourceFormat(buffer_format);
  if (vulkan_implementation->CanImportGpuMemoryBuffer(handle.type)) {
    VkDevice vk_device = context_state->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetVulkanDevice();
    VkImage vk_image = VK_NULL_HANDLE;
    VkImageCreateInfo vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    VkDeviceMemory vk_device_memory = VK_NULL_HANDLE;
    VkDeviceSize memory_size = 0;

    if (!vulkan_implementation->CreateImageFromGpuMemoryHandle(
            vk_device, std::move(handle), size, &vk_image, &vk_image_info,
            &vk_device_memory, &memory_size)) {
      DLOG(ERROR) << "Failed to create VkImage from GpuMemoryHandle.";
      return nullptr;
    }

    VkFormat expected_format = ToVkFormat(resource_format);
    if (expected_format != vk_image_info.format) {
      DLOG(ERROR) << "BufferFormat doesn't match the buffer";
      vkFreeMemory(vk_device, vk_device_memory, nullptr);
      vkDestroyImage(vk_device, vk_image, nullptr);
      return nullptr;
    }

    // TODO(penghuang): track image layout to avoid unnecessary image layout
    // transition. https://crbug.com/965955
    TransitionToColorAttachment(vk_image, context_state, command_pool);

    return base::WrapUnique(new ExternalVkImageBacking(
        mailbox, viz::GetResourceFormat(buffer_format), size, color_space,
        usage, context_state, vk_image, vk_device_memory, memory_size,
        vk_image_info.format, command_pool));
  }

  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
    return nullptr;

  int32_t width_in_bytes = 0;
  if (!viz::ResourceSizes::MaybeWidthInBytes(size.width(), resource_format,
                                             &width_in_bytes)) {
    DLOG(ERROR) << "ResourceSizes::MaybeWidthInBytes() failed.";
    return nullptr;
  }

  if (handle.stride < width_in_bytes) {
    DLOG(ERROR) << "Invalid GMB stride.";
    return nullptr;
  }

  auto bits_per_pixel = viz::BitsPerPixel(resource_format);
  switch (bits_per_pixel) {
    case 64:
    case 32:
    case 16:
      if (handle.stride % (bits_per_pixel / 8) != 0) {
        DLOG(ERROR) << "Invalid GMB stride.";
        return nullptr;
      }
      break;
    case 8:
    case 4:
      break;
    case 12:
      // We are not supporting YVU420 and YUV_420_BIPLANAR format.
    default:
      NOTREACHED();
      return nullptr;
  }

  if (!handle.region.IsValid()) {
    DLOG(ERROR) << "Invalid GMB shared memory region.";
    return nullptr;
  }

  base::CheckedNumeric<size_t> checked_size = handle.stride;
  checked_size *= size.height();
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return nullptr;
  }

  // Minimize the amount of address space we use but make sure offset is a
  // multiple of page size as required by MapAt().
  size_t memory_offset =
      handle.offset % base::SysInfo::VMAllocationGranularity();
  size_t map_offset =
      base::SysInfo::VMAllocationGranularity() *
      (handle.offset / base::SysInfo::VMAllocationGranularity());
  checked_size += memory_offset;
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return nullptr;
  }

  auto shared_memory_mapping = handle.region.MapAt(
      static_cast<off_t>(map_offset), checked_size.ValueOrDie());

  if (!shared_memory_mapping.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory.";
    return nullptr;
  }

  auto backing = Create(context_state, command_pool, mailbox, resource_format,
                        size, color_space, usage, base::span<const uint8_t>(),
                        true /* using_gmb */);
  if (!backing)
    return nullptr;

  backing->InstallSharedMemory(std::move(shared_memory_mapping), handle.stride,
                               memory_offset);
  return backing;
}

ExternalVkImageBacking::ExternalVkImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    SharedContextState* context_state,
    VkImage image,
    VkDeviceMemory memory,
    size_t memory_size,
    VkFormat vk_format,
    VulkanCommandPool* command_pool)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         usage,
                         memory_size,
                         false /* is_thread_safe */),
      context_state_(context_state),
      image_(image),
      memory_(memory),
      memory_size_(memory_size),
      vk_format_(vk_format),
      command_pool_(command_pool) {}

ExternalVkImageBacking::~ExternalVkImageBacking() {
  DCHECK(image_ == VK_NULL_HANDLE);
  DCHECK(memory_ == VK_NULL_HANDLE);
}

bool ExternalVkImageBacking::BeginAccess(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles) {
  if (readonly) {
    if (reads_in_progress_ == 0 && shared_memory_mapping_.IsValid() &&
        shared_memory_is_updated_) {
      if (!WritePixels(
              shared_memory_mapping_.GetMemoryAsSpan<const uint8_t>().subspan(
                  memory_offset_),
              stride_))
        return false;
      shared_memory_is_updated_ = false;
    }
  }
  return BeginAccessInternal(readonly, semaphore_handles);
}

void ExternalVkImageBacking::EndAccess(bool readonly,
                                       SemaphoreHandle semaphore_handle) {
  EndAccessInternal(readonly, std::move(semaphore_handle));
  // TODO(penghuang): read pixels back from VkImage to shared memory GMB, if
  // this feature is needed.
}

bool ExternalVkImageBacking::IsCleared() const {
  return is_cleared_;
}

void ExternalVkImageBacking::SetCleared() {
  is_cleared_ = true;
}

void ExternalVkImageBacking::Update() {
  shared_memory_is_updated_ = true;
}

void ExternalVkImageBacking::Destroy() {
  auto* fence_helper = context_state()
                           ->vk_context_provider()
                           ->GetDeviceQueue()
                           ->GetFenceHelper();
  fence_helper->EnqueueImageCleanupForSubmittedWork(image_, memory_);
  image_ = VK_NULL_HANDLE;
  memory_ = VK_NULL_HANDLE;

  if (texture_) {
    // Ensure that a context is current before removing the ref and calling
    // glDeleteTextures.
    if (!context_state()->context()->IsCurrent(nullptr))
      context_state()->context()->MakeCurrent(context_state()->surface());
    texture_->RemoveLightweightRef(have_context());
  }
}

bool ExternalVkImageBacking::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // It is not safe to produce a legacy mailbox because it would bypass the
  // synchronization between Vulkan and GL that is implemented in the
  // representation classes.
  return false;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
ExternalVkImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  if (!(usage() & SHARED_IMAGE_USAGE_GLES2)) {
    DLOG(ERROR) << "The backing is not created with GLES2 usage.";
    return nullptr;
  }

#if defined(OS_FUCHSIA)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#elif defined(OS_LINUX)
  if (!texture_) {
    VkMemoryGetFdInfoKHR get_fd_info;
    get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    get_fd_info.pNext = nullptr;
    get_fd_info.memory = memory_;
    get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    int memory_fd = -1;
    vkGetMemoryFdKHR(device(), &get_fd_info, &memory_fd);
    if (memory_fd < 0) {
      DLOG(ERROR)
          << "Unable to extract file descriptor out of external VkImage";
      return nullptr;
    }

    gl::GLApi* api = gl::g_current_gl_context;

    constexpr GLenum target = GL_TEXTURE_2D;
    constexpr GLenum get_target = GL_TEXTURE_BINDING_2D;
    GLuint internal_format = viz::TextureStorageFormat(format());

    GLuint memory_object;
    api->glCreateMemoryObjectsEXTFn(1, &memory_object);
    api->glImportMemoryFdEXTFn(memory_object, memory_size_,
                               GL_HANDLE_TYPE_OPAQUE_FD_EXT, memory_fd);
    GLuint texture_service_id;
    api->glGenTexturesFn(1, &texture_service_id);

    GLint old_texture_binding = 0;
    api->glGetIntegervFn(get_target, &old_texture_binding);
    api->glBindTextureFn(target, texture_service_id);
    api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    api->glTexStorageMem2DEXTFn(GL_TEXTURE_2D, 1, internal_format,
                                size().width(), size().height(), memory_object,
                                0);

    texture_ = new gles2::Texture(texture_service_id);
    texture_->SetLightweightRef();
    texture_->SetTarget(target, 1);
    texture_->sampler_state_.min_filter = GL_LINEAR;
    texture_->sampler_state_.mag_filter = GL_LINEAR;
    texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (is_cleared_)
      cleared_rect = gfx::Rect(size());

    GLenum gl_format = viz::GLDataFormat(format());
    GLenum gl_type = viz::GLDataType(format());
    texture_->SetLevelInfo(target, 0, internal_format, size().width(),
                           size().height(), 1, 0, gl_format, gl_type,
                           cleared_rect);
    texture_->SetImmutable(true);

    api->glBindTextureFn(target, old_texture_binding);
  }
  return std::make_unique<ExternalVkImageGlRepresentation>(
      manager, this, tracker, texture_, texture_->service_id());
#else  // !defined(OS_LINUX) && !defined(OS_FUCHSIA)
#error Unsupported OS
#endif
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
ExternalVkImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // Passthrough command decoder is not currently used on Linux.
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia>
ExternalVkImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  // This backing type is only used when vulkan is enabled, so SkiaRenderer
  // should also be using Vulkan.
  DCHECK_EQ(context_state_, context_state.get());
  DCHECK(context_state->GrContextIsVulkan());
  return std::make_unique<ExternalVkImageSkiaRepresentation>(manager, this,
                                                             tracker);
}

void ExternalVkImageBacking::InstallSharedMemory(
    base::WritableSharedMemoryMapping shared_memory_mapping,
    size_t stride,
    size_t memory_offset) {
  DCHECK(!shared_memory_mapping_.IsValid());
  DCHECK(shared_memory_mapping.IsValid());
  shared_memory_mapping_ = std::move(shared_memory_mapping);
  stride_ = stride;
  memory_offset_ = memory_offset;
  Update();
}

bool ExternalVkImageBacking::WritePixels(
    const base::span<const uint8_t>& pixel_data,
    size_t stride) {
  DCHECK(stride == 0 || size().height() * stride <= pixel_data.size());
  VkBufferCreateInfo buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = pixel_data.size(),
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkBuffer stage_buffer = VK_NULL_HANDLE;
  // TODO: Consider reusing stage_buffer and stage_memory, if allocation causes
  // performance issue.
  VkResult result = vkCreateBuffer(device(), &buffer_create_info,
                                   nullptr /* pAllocator */, &stage_buffer);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateBuffer() failed." << result;
    return false;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device(), stage_buffer, &memory_requirements);

  VkMemoryAllocateInfo memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memory_requirements.size,
      .memoryTypeIndex =
          FindMemoryTypeIndex(context_state_, memory_requirements,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),

  };
  VkDeviceMemory stage_memory = VK_NULL_HANDLE;
  result = vkAllocateMemory(device(), &memory_allocate_info,
                            nullptr /* pAllocator */, &stage_memory);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkAllocateMemory() failed. " << result;
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    return false;
  }

  result = vkBindBufferMemory(device(), stage_buffer, stage_memory,
                              0 /* memoryOffset */);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkBindBufferMemory() failed. " << result;
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    vkFreeMemory(device(), stage_memory, nullptr /* pAllocator */);
    return false;
  }

  void* data = nullptr;
  result = vkMapMemory(device(), stage_memory, 0 /* memoryOffset */,
                       pixel_data.size(), 0, &data);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkMapMemory() failed. " << result;
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    vkFreeMemory(device(), stage_memory, nullptr /* pAllocator */);
    return false;
  }
  memcpy(data, pixel_data.data(), pixel_data.size());
  vkUnmapMemory(device(), stage_memory);

  auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer->Initialize());

  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);

    // TODO(penghuang): track image layout to avoid unnecessary image layout
    // transition. https://crbug.com/965955
    command_buffer->TransitionImageLayout(
        image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    uint32_t buffer_width =
        stride ? stride * 8 / BitsPerPixel(format()) : size().width();
    command_buffer->CopyBufferToImage(stage_buffer, image(), buffer_width,
                                      size().height(), size().width(),
                                      size().height());

    // TODO(penghuang): track image layout to avoid unnecessary image layout
    // transition. https://crbug.com/965955
    command_buffer->TransitionImageLayout(
        image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  std::vector<gpu::SemaphoreHandle> handles;
  if (!BeginAccessInternal(false /* readonly */, &handles)) {
    DLOG(ERROR) << "BeginAccess() failed.";
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    vkFreeMemory(device(), stage_memory, nullptr /* pAllocator */);
    return false;
  }

  if (!need_sychronization()) {
    DCHECK(handles.empty());
    command_buffer->Submit(0, nullptr, 0, nullptr);
    EndAccessInternal(false /* readonly */, SemaphoreHandle());

    auto* fence_helper = context_state_->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(command_buffer));
    fence_helper->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                       stage_memory);

    return true;
  }

  std::vector<VkSemaphore> begin_access_semaphores;
  begin_access_semaphores.reserve(handles.size() + 1);
  for (auto& handle : handles) {
    VkSemaphore semaphore = vulkan_implementation()->ImportSemaphoreHandle(
        device(), std::move(handle));
    begin_access_semaphores.emplace_back(semaphore);
  }

  VkSemaphore end_access_semaphore =
      vulkan_implementation()->CreateExternalSemaphore(device());
  command_buffer->Submit(begin_access_semaphores.size(),
                         begin_access_semaphores.data(), 1,
                         &end_access_semaphore);

  auto end_access_semphore_handle = vulkan_implementation()->GetSemaphoreHandle(
      device(), end_access_semaphore);
  EndAccessInternal(false /* readonly */,
                    std::move(end_access_semphore_handle));

  auto* fence_helper =
      context_state_->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(command_buffer));
  begin_access_semaphores.emplace_back(end_access_semaphore);
  fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
      begin_access_semaphores);
  fence_helper->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                     stage_memory);

  return true;
}

bool ExternalVkImageBacking::BeginAccessInternal(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles) {
  DCHECK(semaphore_handles);
  DCHECK(semaphore_handles->empty());
  if (is_write_in_progress_) {
    DLOG(ERROR) << "Unable to begin read or write access because another write "
                   "access is in progress";
    return false;
  }

  if (reads_in_progress_ && !readonly) {
    DLOG(ERROR)
        << "Unable to begin write access because a read access is in progress";
    return false;
  }

  if (readonly) {
    DLOG_IF(ERROR, reads_in_progress_)
        << "Concurrent reading may cause problem.";
    ++reads_in_progress_;
    // If a shared image is read repeatedly without any write access,
    // |read_semaphore_handles_| will never be consumed and released, and then
    // chrome will run out of file descriptors. To avoid this problem, we wait
    // on read semaphores for readonly access too. And in most cases, a shared
    // image is only read from one vulkan device queue, so it should not have
    // performance impact.
    // TODO(penghuang): avoid waiting on read semaphores.
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();

    // A semaphore will become unsignaled, when it has been signaled and waited,
    // so it is not safe to reuse it.
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  } else {
    is_write_in_progress_ = true;
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  }
  return true;
}

void ExternalVkImageBacking::EndAccessInternal(
    bool readonly,
    SemaphoreHandle semaphore_handle) {
  if (readonly) {
    DCHECK_GT(reads_in_progress_, 0u);
    --reads_in_progress_;
  } else {
    DCHECK(is_write_in_progress_);
    is_write_in_progress_ = false;
  }

  if (need_sychronization()) {
    DCHECK(semaphore_handle.is_valid());
    if (readonly) {
      read_semaphore_handles_.push_back(std::move(semaphore_handle));
    } else {
      DCHECK(!write_semaphore_handle_.is_valid());
      DCHECK(read_semaphore_handles_.empty());
      write_semaphore_handle_ = std::move(semaphore_handle);
    }
  } else {
    DCHECK(!semaphore_handle.is_valid());
  }
}

}  // namespace gpu
