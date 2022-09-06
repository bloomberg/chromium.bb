// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_dawn_egl_image.h"

#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"

#include <dawn/native/OpenGLBackend.h>

namespace {
GLenum ToSharedImageAccessGLMode(WGPUTextureUsage usage) {
  if (usage & (WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment |
               WGPUTextureUsage_StorageBinding | WGPUTextureUsage_Present)) {
    return GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  } else {
    return GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM;
  }
}
}  // namespace

namespace gpu {

SharedImageRepresentationDawnEGLImage::SharedImageRepresentationDawnEGLImage(
    std::unique_ptr<SharedImageRepresentationGLTextureBase> gl_representation,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      gl_representation_(std::move(gl_representation)),
      device_(device),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid.
  dawn_procs_.deviceReference(device_);
}

SharedImageRepresentationDawnEGLImage::
    ~SharedImageRepresentationDawnEGLImage() {
  EndAccess();

  dawn_procs_.deviceRelease(device_);
}

WGPUTexture SharedImageRepresentationDawnEGLImage::BeginAccess(
    WGPUTextureUsage usage) {
  gl_representation_->BeginAccess(ToSharedImageAccessGLMode(usage));
  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = viz::ToWGPUFormat(format());
  texture_descriptor.usage = WGPUTextureUsage_CopySrc |
                             WGPUTextureUsage_CopyDst |
                             WGPUTextureUsage_RenderAttachment;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  dawn::native::opengl::ExternalImageDescriptorEGLImage externalImageDesc;
  externalImageDesc.cTextureDescriptor = &texture_descriptor;
  const gl::GLImage* image;
  gpu::TextureBase* texture = gl_representation_->GetTextureBase();
  if (texture->GetType() == gpu::TextureBase::Type::kPassthrough) {
    image = static_cast<gles2::TexturePassthrough*>(texture)->GetLevelImage(
        texture->target(), 0u);
  } else {
    image = static_cast<gles2::Texture*>(texture)->GetLevelImage(
        texture->target(), 0u);
  }
  externalImageDesc.image = image->GetEGLImage();
  DCHECK(externalImageDesc.image);
  externalImageDesc.isInitialized = true;
  texture_ =
      dawn::native::opengl::WrapExternalEGLImage(device_, &externalImageDesc);
  return texture_;
}

void SharedImageRepresentationDawnEGLImage::EndAccess() {
  if (!texture_) {
    return;
  }
  if (dawn::native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }
  gl_representation_->EndAccess();
  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);
  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu
