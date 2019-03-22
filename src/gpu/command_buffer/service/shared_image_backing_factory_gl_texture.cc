// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include "base/feature_list.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/trace_util.h"

namespace gpu {

namespace {

class ScopedRestoreTexture {
 public:
  ScopedRestoreTexture(gl::GLApi* api, GLenum target)
      : api_(api), target_(target) {
    GLenum get_target = GL_TEXTURE_BINDING_2D;
    switch (target) {
      case GL_TEXTURE_2D:
        get_target = GL_TEXTURE_BINDING_2D;
        break;
      case GL_TEXTURE_RECTANGLE_ARB:
        get_target = GL_TEXTURE_BINDING_RECTANGLE_ARB;
        break;
      case GL_TEXTURE_EXTERNAL_OES:
        get_target = GL_TEXTURE_BINDING_EXTERNAL_OES;
        break;
      default:
        NOTREACHED();
        break;
    }
    GLint old_texture_binding = 0;
    api->glGetIntegervFn(get_target, &old_texture_binding);
    old_binding_ = old_texture_binding;
  }

  ~ScopedRestoreTexture() { api_->glBindTextureFn(target_, old_binding_); }

 private:
  gl::GLApi* api_;
  GLenum target_;
  GLuint old_binding_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedRestoreTexture);
};

GLuint MakeTextureAndSetParameters(gl::GLApi* api,
                                   GLenum target,
                                   bool framebuffer_attachment_angle) {
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (framebuffer_attachment_angle) {
    api->glTexParameteriFn(target, GL_TEXTURE_USAGE_ANGLE,
                           GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }
  return service_id;
}

}  // anonymous namespace

// Representation of a SharedImageBackingGLTexture as a GL Texture.
class SharedImageRepresentationGLTextureImpl
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureImpl(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker,
                                         gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }

 private:
  gles2::Texture* texture_;
};

// Representation of a SharedImageBackingGLTexturePassthrough as a GL
// TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughImpl
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_passthrough_(std::move(texture_passthrough)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_passthrough_;
  }

 private:
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
};

class SharedImageRepresentationSkiaImpl : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaImpl(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker,
                                    GLenum target,
                                    GLenum internal_format,
                                    GLenum driver_internal_format,
                                    GLuint service_id)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        target_(target),
        internal_format_(internal_format),
        driver_internal_format_(driver_internal_format),
        service_id_(service_id) {}

  ~SharedImageRepresentationSkiaImpl() override { DCHECK(!write_surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      GrContext* gr_context,
      int final_msaa_count,
      SkColorType color_type,
      const SkSurfaceProps& surface_props) override {
    if (write_surface_)
      return nullptr;

    GrBackendTexture backend_texture;
    if (!GetGrBackendTexture(target_, size(), internal_format_,
                             driver_internal_format_, service_id_, color_type,
                             &backend_texture)) {
      return nullptr;
    }
    auto surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, final_msaa_count,
        color_type, nullptr, &surface_props);
    write_surface_ = surface.get();
    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_ = nullptr;
  }

  bool BeginReadAccess(SkColorType color_type,
                       GrBackendTexture* backend_texture) override {
    if (!GetGrBackendTexture(target_, size(), internal_format_,
                             driver_internal_format_, service_id_, color_type,
                             backend_texture)) {
      return false;
    }
    return true;
  }

  void EndReadAccess() override {
    // TODO(ericrk): Handle begin/end correctness checks.
  }

 private:
  GLenum target_;
  GLenum internal_format_ = 0;
  GLenum driver_internal_format_ = 0;
  GLuint service_id_;

  SkSurface* write_surface_ = nullptr;
};

// Implementation of SharedImageBacking that creates a GL Texture and stores it
// as a gles2::Texture. Can be used with the legacy mailbox implementation.
class SharedImageBackingGLTexture : public SharedImageBacking {
 public:
  SharedImageBackingGLTexture(const Mailbox& mailbox,
                              viz::ResourceFormat format,
                              const gfx::Size& size,
                              const gfx::ColorSpace& color_space,
                              uint32_t usage,
                              gles2::Texture* texture,
                              GLenum internal_format,
                              GLenum driver_internal_format)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           texture->estimated_size()),
        texture_(texture),
        internal_format_(internal_format),
        driver_internal_format_(driver_internal_format) {
    DCHECK(texture_);
  }

  ~SharedImageBackingGLTexture() override { DCHECK(!texture_); }

  bool IsCleared() const override {
    return texture_->IsLevelCleared(texture_->target(), 0);
  }

  void SetCleared() override {
    texture_->SetLevelCleared(texture_->target(), 0, true);
  }

  void Update() override {
    GLenum target = texture_->target();
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);
    api->glBindTextureFn(target, texture_->service_id());

    gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
    gl::GLImage* image = texture_->GetLevelImage(target, 0, &old_state);
    if (!image)
      return;
    image->ReleaseTexImage(target);
    gles2::Texture::ImageState new_state = gles2::Texture::UNBOUND;
    if (image->BindTexImage(target))
      new_state = gles2::Texture::BOUND;
    if (old_state != new_state)
      texture_->SetLevelImage(target, 0, image, new_state);
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    DCHECK(texture_);
    mailbox_manager->ProduceTexture(mailbox(), texture_);
    return true;
  }

  void Destroy() override {
    DCHECK(texture_);
    texture_->RemoveLightweightRef(have_context());
    texture_ = nullptr;
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.

    pmd->AddOwnershipEdge(client_guid, service_guid, importance);

    // Dump all sub-levels held by the texture. They will appear below the
    // main gl/textures/client_X/mailbox_Y dump.
    texture_->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    return std::make_unique<SharedImageRepresentationGLTextureImpl>(
        manager, this, tracker, texture_);
  }
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    return std::make_unique<SharedImageRepresentationSkiaImpl>(
        manager, this, tracker, texture_->target(), internal_format_,
        driver_internal_format_, texture_->service_id());
  }

 private:
  gles2::Texture* texture_ = nullptr;
  GLenum internal_format_ = 0;
  GLenum driver_internal_format_ = 0;
};

// Implementation of SharedImageBacking that creates a GL Texture and stores it
// as a gles2::TexturePassthrough. Can be used with the legacy mailbox
// implementation.
class SharedImageBackingPassthroughGLTexture : public SharedImageBacking {
 public:
  SharedImageBackingPassthroughGLTexture(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      scoped_refptr<gles2::TexturePassthrough> passthrough_texture,
      GLenum internal_format,
      GLenum driver_internal_format)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           passthrough_texture->estimated_size()),
        texture_passthrough_(std::move(passthrough_texture)),
        internal_format_(internal_format),
        driver_internal_format_(driver_internal_format) {
    DCHECK(texture_passthrough_);
  }

  ~SharedImageBackingPassthroughGLTexture() override {
    DCHECK(!texture_passthrough_);
  }

  bool IsCleared() const override { return is_cleared_; }
  void SetCleared() override { is_cleared_ = true; }

  void Update() override {
    GLenum target = texture_passthrough_->target();
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);
    api->glBindTextureFn(target, texture_passthrough_->service_id());

    gl::GLImage* image = texture_passthrough_->GetLevelImage(target, 0);
    if (!image)
      return;
    image->ReleaseTexImage(target);
    if (!image->BindTexImage(target))
      image->CopyTexImage(target);
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    DCHECK(texture_passthrough_);
    mailbox_manager->ProduceTexture(mailbox(), texture_passthrough_.get());
    return true;
  }

  void Destroy() override {
    DCHECK(texture_passthrough_);
    if (!have_context())
      texture_passthrough_->MarkContextLost();
    texture_passthrough_.reset();
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
        manager, this, tracker, texture_passthrough_);
  }
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    return std::make_unique<SharedImageRepresentationSkiaImpl>(
        manager, this, tracker, texture_passthrough_->target(),
        internal_format_, driver_internal_format_,
        texture_passthrough_->service_id());
  }

 private:
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  GLenum internal_format_ = 0;
  GLenum driver_internal_format_ = 0;
  bool is_cleared_ = false;
};

SharedImageBackingFactoryGLTexture::SharedImageBackingFactoryGLTexture(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    ImageFactory* image_factory)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      image_factory_(image_factory) {
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  if (workarounds.max_texture_size) {
    max_texture_size_ =
        std::min(max_texture_size_, workarounds.max_texture_size);
  }

  // TODO(piman): Can we extract the logic out of FeatureInfo?
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough_, gles2::DisallowedFeatures());
  gpu_memory_buffer_formats_ =
      feature_info->feature_flags().gpu_memory_buffer_formats;
  texture_usage_angle_ = feature_info->feature_flags().angle_texture_usage;
  es3_capable_ = feature_info->IsES3Capable();
  bool enable_texture_storage =
      feature_info->feature_flags().ext_texture_storage;
  bool enable_scanout_images =
      (image_factory_ && image_factory_->SupportsCreateAnonymousImage());
  const gles2::Validators* validators = feature_info->validators();
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];
    // TODO(piman): do we need to support ETC1?
    if (!viz::GLSupportsFormat(format) ||
        viz::IsResourceFormatCompressed(format))
      continue;
    GLuint image_internal_format = viz::GLInternalFormat(format);
    GLenum gl_format = viz::GLDataFormat(format);
    GLenum gl_type = viz::GLDataType(format);
    if (validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(gl_format) &&
        validators->pixel_type.IsValid(gl_type)) {
      info.enabled = true;
      info.gl_format = gl_format;
      info.gl_type = gl_type;
      info.swizzle = gles2::TextureManager::GetCompatibilitySwizzle(
          feature_info.get(), gl_format);
      info.image_internal_format =
          gles2::TextureManager::AdjustTexInternalFormat(feature_info.get(),
                                                         image_internal_format);
      info.adjusted_format =
          gles2::TextureManager::AdjustTexFormat(feature_info.get(), gl_format);
    }
    if (!info.enabled)
      continue;
    if (enable_texture_storage) {
      GLuint storage_internal_format = viz::TextureStorageFormat(format);
      if (validators->texture_internal_format_storage.IsValid(
              storage_internal_format)) {
        info.supports_storage = true;
        info.storage_internal_format =
            gles2::TextureManager::AdjustTexStorageFormat(
                feature_info.get(), storage_internal_format);
      }
    }
    if (!info.enabled || !enable_scanout_images ||
        !IsGpuMemoryBufferFormatSupported(format))
      continue;
    gfx::BufferFormat buffer_format = viz::BufferFormat(format);
    switch (buffer_format) {
      case gfx::BufferFormat::RGBA_8888:
      case gfx::BufferFormat::BGRA_8888:
      case gfx::BufferFormat::RGBA_F16:
      case gfx::BufferFormat::R_8:
        break;
      default:
        continue;
    }
    info.allow_scanout = true;
    info.buffer_format = buffer_format;
    DCHECK_EQ(info.gl_format,
              gpu::InternalFormatForGpuMemoryBufferFormat(buffer_format));
    if (base::ContainsValue(gpu_preferences.texture_target_exception_list,
                            gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                                      buffer_format)))
      info.target_for_scanout = gpu::GetPlatformSpecificTextureTarget();
  }
}

SharedImageBackingFactoryGLTexture::~SharedImageBackingFactoryGLTexture() =
    default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  const FormatInfo& format_info = format_info_[format];
  if (!format_info.enabled) {
    LOG(ERROR) << "CreateSharedImage: invalid format";
    return nullptr;
  }

  const bool use_buffer = usage & SHARED_IMAGE_USAGE_SCANOUT;
  if (use_buffer && !format_info.allow_scanout) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable";
    return nullptr;
  }

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return nullptr;
  }

  GLenum target = use_buffer ? format_info.target_for_scanout : GL_TEXTURE_2D;

  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  GLuint service_id = MakeTextureAndSetParameters(
      api, target, for_framebuffer_attachment && texture_usage_angle_);

  scoped_refptr<gl::GLImage> image;
  // TODO(piman): We pretend the texture was created in an ES2 context, so that
  // it can be used in other ES2 contexts, and so we have to pass gl_format as
  // the internal format in the LevelInfo. https://crbug.com/628064
  GLuint level_info_internal_format = format_info.gl_format;
  bool is_cleared = false;
  if (use_buffer) {
    image = image_factory_->CreateAnonymousImage(
        size, format_info.buffer_format, gfx::BufferUsage::SCANOUT,
        &is_cleared);
    if (!image || !image->BindTexImage(target)) {
      LOG(ERROR) << "CreateSharedImage: Failed to create image";
      api->glDeleteTexturesFn(1, &service_id);
      return nullptr;
    }
    level_info_internal_format = image->GetInternalFormat();
    if (color_space.IsValid())
      image->SetColorSpace(color_space);
  } else if (format_info.supports_storage) {
    api->glTexStorage2DEXTFn(target, 1, format_info.storage_internal_format,
                             size.width(), size.height());
  } else {
    // Need to unbind any GL_PIXEL_UNPACK_BUFFER for the nullptr in
    // glTexImage2D to mean "no pixels" (as opposed to offset 0 in the
    // buffer).
    GLint bound_pixel_unpack_buffer = 0;
    if (es3_capable_) {
      api->glGetIntegervFn(GL_PIXEL_UNPACK_BUFFER_BINDING,
                           &bound_pixel_unpack_buffer);
      if (bound_pixel_unpack_buffer)
        api->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    api->glTexImage2DFn(target, 0, format_info.image_internal_format,
                        size.width(), size.height(), 0,
                        format_info.adjusted_format, format_info.gl_type,
                        nullptr);
    if (bound_pixel_unpack_buffer)
      api->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, bound_pixel_unpack_buffer);
  }

  return MakeBacking(mailbox, target, service_id, image, gles2::Texture::BOUND,
                     level_info_internal_format, format_info.gl_format,
                     format_info.gl_type, format_info.swizzle, is_cleared,
                     format, size, color_space, usage);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (!gpu_memory_buffer_formats_.Has(buffer_format)) {
    LOG(ERROR) << "CreateSharedImage: unsupported buffer format";
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  GLenum target = handle.type == gfx::SHARED_MEMORY_BUFFER
                      ? GL_TEXTURE_2D
                      : gpu::GetPlatformSpecificTextureTarget();
  scoped_refptr<gl::GLImage> image = MakeGLImage(
      client_id, std::move(handle), buffer_format, surface_handle, size);
  if (!image) {
    LOG(ERROR) << "Failed to create image.";
    return nullptr;
  }
  if (color_space.IsValid())
    image->SetColorSpace(color_space);

  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);

  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  GLuint service_id = MakeTextureAndSetParameters(
      api, target, for_framebuffer_attachment && texture_usage_angle_);

  // TODO(piman): RGB emulation
  gles2::Texture::ImageState image_state = gles2::Texture::UNBOUND;
  if (image->BindTexImage(target)) {
    image_state = gles2::Texture::BOUND;
  } else if (use_passthrough_) {
    image->CopyTexImage(target);
    image_state = gles2::Texture::COPIED;
  }

  // TODO(piman): this is consistent with
  // GLES2DecoderImpl::BindTexImage2DCHROMIUMImpl or
  // RasterDecoderImpl::DoBindTexImage2DCHROMIUM but seems wrong:
  //
  // - internalformat might be sized, which is wrong for format
  // - gl_type shouldn't be GL_UNSIGNED_BYTE for RGBA4444 for example.
  GLuint internal_format = image->GetInternalFormat();
  GLenum gl_format = internal_format;
  GLenum gl_type = GL_UNSIGNED_BYTE;

  return MakeBacking(mailbox, target, service_id, image, image_state,
                     internal_format, gl_format, gl_type, nullptr, true, format,
                     size, color_space, usage);
}

scoped_refptr<gl::GLImage> SharedImageBackingFactoryGLTexture::MakeGLImage(
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size) {
  if (!image_factory_)
    return nullptr;
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
        return nullptr;
      auto image = base::MakeRefCounted<gl::GLImageSharedMemory>(size);
      if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                             handle.stride)) {
        return nullptr;
      }

      return image;
    }
    default:
      return image_factory_->CreateImageForGpuMemoryBuffer(
          std::move(handle), size, format, client_id, surface_handle);
  }
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::MakeBacking(
    const Mailbox& mailbox,
    GLenum target,
    GLuint service_id,
    scoped_refptr<gl::GLImage> image,
    gles2::Texture::ImageState image_state,
    GLuint level_info_internal_format,
    GLuint gl_format,
    GLuint gl_type,
    const gles2::Texture::CompatibilitySwizzle* swizzle,
    bool is_cleared,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  // Calculate |driver_internal_format| here rather than caching on
  // format_info, as we need to use the |level_info_internal_format| which may
  // depend on the generated |image|.
  GLenum driver_internal_format =
      gl::GetInternalFormat(gl::GLContext::GetCurrent()->GetVersionInfo(),
                            level_info_internal_format);
  if (use_passthrough_) {
    scoped_refptr<gles2::TexturePassthrough> passthrough_texture =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
    if (image)
      passthrough_texture->SetLevelImage(target, 0, image.get());

    // Get the texture size from ANGLE and set it on the passthrough texture.
    GLint texture_memory_size = 0;
    gl::GLApi* api = gl::g_current_gl_context;
    api->glGetTexParameterivFn(target, GL_MEMORY_SIZE_ANGLE,
                               &texture_memory_size);
    passthrough_texture->SetEstimatedSize(texture_memory_size);

    return std::make_unique<SharedImageBackingPassthroughGLTexture>(
        mailbox, format, size, color_space, usage,
        std::move(passthrough_texture), level_info_internal_format,
        driver_internal_format);
  } else {
    gles2::Texture* texture = new gles2::Texture(service_id);
    texture->SetLightweightRef();
    texture->SetTarget(target, 1);
    texture->sampler_state_.min_filter = GL_LINEAR;
    texture->sampler_state_.mag_filter = GL_LINEAR;
    texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture->SetLevelInfo(target, 0, level_info_internal_format, size.width(),
                          size.height(), 1, 0, gl_format, gl_type,
                          is_cleared ? gfx::Rect(size) : gfx::Rect());
    if (swizzle)
      texture->SetCompatibilitySwizzle(swizzle);
    if (image)
      texture->SetLevelImage(target, 0, image.get(), image_state);
    texture->SetImmutable(true);

    return std::make_unique<SharedImageBackingGLTexture>(
        mailbox, format, size, color_space, usage, texture,
        level_info_internal_format, driver_internal_format);
  }
}

SharedImageBackingFactoryGLTexture::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryGLTexture::FormatInfo::~FormatInfo() = default;

}  // namespace gpu
