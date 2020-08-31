// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"

namespace gpu {

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  backing_->AddRef(this);
}

SharedImageRepresentation::~SharedImageRepresentation() {
  // CHECK here as we'll crash later anyway, and this makes it clearer what the
  // error is.
  CHECK(!has_scoped_access_) << "Destroying a SharedImageRepresentation with "
                                "outstanding Scoped*Access objects.";
  manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
}

std::unique_ptr<SharedImageRepresentationGLTexture::ScopedAccess>
SharedImageRepresentationGLTextureBase::BeginScopedAccess(
    GLenum mode,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  if (!BeginAccess(mode))
    return nullptr;

  UpdateClearedStateOnBeginAccess();

  constexpr GLenum kReadAccess = 0x8AF6;
  if (mode == kReadAccess)
    backing()->OnReadSucceeded();
  else
    backing()->OnWriteSucceeded();

  return std::make_unique<ScopedAccess>(
      util::PassKey<SharedImageRepresentationGLTextureBase>(), this);
}

bool SharedImageRepresentationGLTextureBase::BeginAccess(GLenum mode) {
  return true;
}

bool SharedImageRepresentationGLTextureBase::
    SupportsMultipleConcurrentReadAccess() {
  return false;
}

gpu::TextureBase* SharedImageRepresentationGLTexture::GetTextureBase() {
  return GetTexture();
}

void SharedImageRepresentationGLTexture::UpdateClearedStateOnEndAccess() {
  auto* texture = GetTexture();
  // Operations on the gles2::Texture may have cleared or uncleared it. Make
  // sure this state is reflected back in the SharedImage.
  gfx::Rect cleared_rect = texture->GetLevelClearedRect(texture->target(), 0);
  if (cleared_rect != ClearedRect())
    SetClearedRect(cleared_rect);
}

void SharedImageRepresentationGLTexture::UpdateClearedStateOnBeginAccess() {
  auto* texture = GetTexture();
  // Operations outside of the gles2::Texture may have cleared or uncleared it.
  // Make sure this state is reflected back in gles2::Texture.
  gfx::Rect cleared_rect = ClearedRect();
  if (cleared_rect != texture->GetLevelClearedRect(texture->target(), 0))
    texture->SetLevelClearedRect(texture->target(), 0, cleared_rect);
}

gpu::TextureBase*
SharedImageRepresentationGLTexturePassthrough::GetTextureBase() {
  return GetTexturePassthrough().get();
}

bool SharedImageRepresentationSkia::SupportsMultipleConcurrentReadAccess() {
  return false;
}

SharedImageRepresentationSkia::ScopedWriteAccess::ScopedWriteAccess(
    util::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkSurface> surface)
    : ScopedAccessBase(representation), surface_(std::move(surface)) {}

SharedImageRepresentationSkia::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess(std::move(surface_));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  sk_sp<SkSurface> surface = BeginWriteAccess(final_msaa_count, surface_props,
                                              begin_semaphores, end_semaphores);
  if (!surface)
    return nullptr;

  return std::make_unique<ScopedWriteAccess>(
      util::PassKey<SharedImageRepresentationSkia>(), this, std::move(surface));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared) {
  return BeginScopedWriteAccess(
      0 /* final_msaa_count */,
      SkSurfaceProps(0 /* flags */, kUnknown_SkPixelGeometry), begin_semaphores,
      end_semaphores, allow_uncleared);
}

SharedImageRepresentationSkia::ScopedReadAccess::ScopedReadAccess(
    util::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkPromiseImageTexture> promise_image_texture)
    : ScopedAccessBase(representation),
      promise_image_texture_(std::move(promise_image_texture)) {}

SharedImageRepresentationSkia::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
SharedImageRepresentationSkia::BeginScopedReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  sk_sp<SkPromiseImageTexture> promise_image_texture =
      BeginReadAccess(begin_semaphores, end_semaphores);
  if (!promise_image_texture)
    return nullptr;

  return std::make_unique<ScopedReadAccess>(
      util::PassKey<SharedImageRepresentationSkia>(), this,
      std::move(promise_image_texture));
}

SharedImageRepresentationOverlay::ScopedReadAccess::ScopedReadAccess(
    util::PassKey<SharedImageRepresentationOverlay> pass_key,
    SharedImageRepresentationOverlay* representation,
    gl::GLImage* gl_image)
    : ScopedAccessBase(representation), gl_image_(gl_image) {}

std::unique_ptr<SharedImageRepresentationOverlay::ScopedReadAccess>
SharedImageRepresentationOverlay::BeginScopedReadAccess(bool needs_gl_image) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  if (!BeginReadAccess())
    return nullptr;

  return std::make_unique<ScopedReadAccess>(
      util::PassKey<SharedImageRepresentationOverlay>(), this,
      needs_gl_image ? GetGLImage() : nullptr);
}

SharedImageRepresentationDawn::ScopedAccess::ScopedAccess(
    util::PassKey<SharedImageRepresentationDawn> /* pass_key */,
    SharedImageRepresentationDawn* representation,
    WGPUTexture texture)
    : ScopedAccessBase(representation), texture_(texture) {}

SharedImageRepresentationDawn::ScopedAccess::~ScopedAccess() {
  representation()->EndAccess();
}

std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess>
SharedImageRepresentationDawn::BeginScopedAccess(
    WGPUTextureUsage usage,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  WGPUTexture texture = BeginAccess(usage);
  if (!texture)
    return nullptr;
  return std::make_unique<ScopedAccess>(
      util::PassKey<SharedImageRepresentationDawn>(), this, texture);
}

SharedImageRepresentationFactoryRef::~SharedImageRepresentationFactoryRef() {
  backing()->UnregisterImageFactory();
  backing()->MarkForDestruction();
}

SharedImageRepresentationVaapi::SharedImageRepresentationVaapi(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    VaapiDependencies* vaapi_deps)
    : SharedImageRepresentation(manager, backing, tracker),
      vaapi_deps_(vaapi_deps) {}

SharedImageRepresentationVaapi::~SharedImageRepresentationVaapi() = default;

SharedImageRepresentationVaapi::ScopedWriteAccess::ScopedWriteAccess(
    util::PassKey<SharedImageRepresentationVaapi> /* pass_key */,
    SharedImageRepresentationVaapi* representation)
    : ScopedAccessBase(representation) {}

SharedImageRepresentationVaapi::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndAccess();
}

const media::VASurface*
SharedImageRepresentationVaapi::ScopedWriteAccess::va_surface() {
  return representation()->vaapi_deps_->GetVaSurface();
}

std::unique_ptr<SharedImageRepresentationVaapi::ScopedWriteAccess>
SharedImageRepresentationVaapi::BeginScopedWriteAccess() {
  return std::make_unique<ScopedWriteAccess>(
      util::PassKey<SharedImageRepresentationVaapi>(), this);
}

}  // namespace gpu
