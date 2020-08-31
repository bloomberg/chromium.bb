// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/skia_limits.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

#if defined(OS_ANDROID)
#include "gpu/config/gpu_finch_features.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#endif

namespace gpu {

namespace {

struct FlushCleanupContext {
  std::vector<base::OnceClosure> cleanup_tasks;
};

void CleanupAfterSkiaFlush(void* context) {
  FlushCleanupContext* flush_context =
      static_cast<FlushCleanupContext*>(context);
  for (auto& task : flush_context->cleanup_tasks) {
    std::move(task).Run();
  }
  delete flush_context;
}

template <class T>
void DeleteSkObject(SharedContextState* context_state, sk_sp<T> sk_object) {
  DCHECK(sk_object && sk_object->unique());

  if (context_state->context_lost())
    return;
  DCHECK(!context_state->gr_context()->abandoned());

  if (!context_state->GrContextIsVulkan())
    return;

#if BUILDFLAG(ENABLE_VULKAN)
  auto* fence_helper =
      context_state->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](const sk_sp<GrContext>& gr_context, sk_sp<T> sk_object,
         gpu::VulkanDeviceQueue* device_queue, bool is_lost) {},
      sk_ref_sp(context_state->gr_context()), std::move(sk_object)));
#endif
}

}  // namespace

GrContextOptions GetDefaultGrContextOptions(GrContextType type) {
  // If you make any changes to the GrContext::Options here that could affect
  // text rendering, make sure to match the capabilities initialized in
  // GetCapabilities and ensuring these are also used by the
  // PaintOpBufferSerializer.
  GrContextOptions options;
  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &glyph_cache_max_texture_bytes);
  options.fDisableCoverageCountingPaths = true;
  options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;
  // TODO(csmartdalton): enable internal multisampling after the related Skia
  // rolls are in.
  options.fInternalMultisampleCount = 0;
  if (type == GrContextType::kMetal)
    options.fRuntimeProgramCacheSize = 1024;
  return options;
}

GLuint GetGrGLBackendTextureFormat(const gles2::FeatureInfo* feature_info,
                                   viz::ResourceFormat resource_format) {
  const gl::GLVersionInfo* version_info = &feature_info->gl_version_info();
  GLuint internal_format = gl::GetInternalFormat(
      version_info, viz::TextureStorageFormat(resource_format));

  bool use_version_es2 = false;
#if defined(OS_ANDROID)
  use_version_es2 = base::FeatureList::IsEnabled(features::kUseGles2ForOopR);
#endif

  // We tell Skia to use es2 which does not have GL_R8_EXT
  if (feature_info->gl_version_info().is_es3 && use_version_es2) {
    if (internal_format == GL_R8_EXT)
      internal_format = GL_LUMINANCE8;
  }

  return internal_format;
}

bool GetGrBackendTexture(const gles2::FeatureInfo* feature_info,
                         GLenum target,
                         const gfx::Size& size,
                         GLuint service_id,
                         viz::ResourceFormat resource_format,
                         GrBackendTexture* gr_texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE_ARB &&
      target != GL_TEXTURE_EXTERNAL_OES) {
    LOG(ERROR) << "GetGrBackendTexture: invalid texture target.";
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = service_id;
  texture_info.fTarget = target;
  texture_info.fFormat =
      GetGrGLBackendTextureFormat(feature_info, resource_format);
  *gr_texture = GrBackendTexture(size.width(), size.height(), GrMipMapped::kNo,
                                 texture_info);
  return true;
}

void AddCleanupTaskForSkiaFlush(base::OnceClosure task,
                                GrFlushInfo* flush_info) {
  FlushCleanupContext* context;
  if (!flush_info->fFinishedProc) {
    DCHECK(!flush_info->fFinishedContext);
    flush_info->fFinishedProc = &CleanupAfterSkiaFlush;
    context = new FlushCleanupContext();
    flush_info->fFinishedContext = context;
  } else {
    DCHECK_EQ(flush_info->fFinishedProc, &CleanupAfterSkiaFlush);
    DCHECK(flush_info->fFinishedContext);
    context = static_cast<FlushCleanupContext*>(flush_info->fFinishedContext);
  }
  context->cleanup_tasks.push_back(std::move(task));
}

void AddVulkanCleanupTaskForSkiaFlush(
    viz::VulkanContextProvider* context_provider,
    GrFlushInfo* flush_info) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (context_provider) {
    auto task = context_provider->GetDeviceQueue()
                    ->GetFenceHelper()
                    ->CreateExternalCallback();
    if (task)
      AddCleanupTaskForSkiaFlush(std::move(task), flush_info);
  }
#endif
}

void DeleteGrBackendTexture(SharedContextState* context_state,
                            GrBackendTexture* backend_texture) {
  DCHECK(backend_texture && backend_texture->isValid());

  if (context_state->context_lost())
    return;
  DCHECK(!context_state->gr_context()->abandoned());

  if (!context_state->GrContextIsVulkan()) {
    context_state->gr_context()->deleteBackendTexture(
        std::move(*backend_texture));
    return;
  }

#if BUILDFLAG(ENABLE_VULKAN)
  auto* fence_helper =
      context_state->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](const sk_sp<GrContext>& gr_context, GrBackendTexture backend_texture,
         gpu::VulkanDeviceQueue* device_queue, bool is_lost) {
        if (!gr_context->abandoned())
          gr_context->deleteBackendTexture(std::move(backend_texture));
      },
      sk_ref_sp(context_state->gr_context()), std::move(*backend_texture)));
#endif
}

void DeleteSkImage(SharedContextState* context_state, sk_sp<SkImage> sk_image) {
  DeleteSkObject(context_state, std::move(sk_image));
}

void DeleteSkSurface(SharedContextState* context_state,
                     sk_sp<SkSurface> sk_surface) {
  DeleteSkObject(context_state, std::move(sk_surface));
}

#if BUILDFLAG(ENABLE_VULKAN)
GrVkImageInfo CreateGrVkImageInfo(VulkanImage* image) {
  DCHECK(image);
  VkPhysicalDevice physical_device =
      image->device_queue()->GetVulkanPhysicalDevice();
  GrVkYcbcrConversionInfo gr_ycbcr_info = CreateGrVkYcbcrConversionInfo(
      physical_device, image->image_tiling(), image->ycbcr_info());
  GrVkAlloc alloc(image->device_memory(), /*offset=*/0, image->device_size(),
                  /*flags=*/0);
  bool is_protected = image->flags() & VK_IMAGE_CREATE_PROTECTED_BIT;
  return GrVkImageInfo(
      image->image(), alloc, image->image_tiling(), image->image_layout(),
      image->format(), /*levelCount=*/1, image->queue_family_index(),
      is_protected ? GrProtected::kYes : GrProtected::kNo, gr_ycbcr_info);
}

GrVkYcbcrConversionInfo CreateGrVkYcbcrConversionInfo(
    VkPhysicalDevice physical_device,
    VkImageTiling tiling,
    const base::Optional<VulkanYCbCrInfo>& ycbcr_info) {
  if (!ycbcr_info)
    return GrVkYcbcrConversionInfo();

  VkFormat vk_format = static_cast<VkFormat>(ycbcr_info->image_format);
  VkFormatFeatureFlags format_features =
      static_cast<VkFormatFeatureFlags>(ycbcr_info->format_features);

  // |format_features| is expected to be set for external images. For regular
  // (non-external) images it may be set to 0. In that case we need to get the
  // value from Vulkan.
  if (format_features == 0) {
    DCHECK_NE(vk_format, 0);
    VkFormatProperties format_props = {};

    // vkGetPhysicalDeviceFormatProperties() is safe to call on any thread.
    vkGetPhysicalDeviceFormatProperties(physical_device, vk_format,
                                        &format_props);
    format_features = (tiling == VK_IMAGE_TILING_LINEAR)
                          ? format_props.linearTilingFeatures
                          : format_props.optimalTilingFeatures;
  }

  // As per the spec here [1], if the format does not support
  // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT,
  // chromaFilter must be VK_FILTER_NEAREST.
  // [1] -
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkSamplerYcbcrConversionCreateInfo.html.
  VkFilter chroma_filter =
      (format_features &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT)
          ? VK_FILTER_LINEAR
          : VK_FILTER_NEAREST;

  return GrVkYcbcrConversionInfo(
      vk_format, ycbcr_info->external_format,
      static_cast<VkSamplerYcbcrModelConversion>(
          ycbcr_info->suggested_ycbcr_model),
      static_cast<VkSamplerYcbcrRange>(ycbcr_info->suggested_ycbcr_range),
      static_cast<VkChromaLocation>(ycbcr_info->suggested_xchroma_offset),
      static_cast<VkChromaLocation>(ycbcr_info->suggested_ychroma_offset),
      chroma_filter,
      /*forceExplicitReconstruction=*/false, format_features);
}

#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace gpu
