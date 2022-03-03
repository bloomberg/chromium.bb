// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_d3d.h"

#include <d3d11_3.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image_representation_d3d.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image_representation_dawn_egl_image.h"
#endif

namespace gpu {

namespace {

bool SupportsVideoFormat(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return true;
    default:
      return false;
  }
}

size_t NumPlanes(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
      return 2;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

viz::ResourceFormat PlaneFormat(DXGI_FORMAT dxgi_format, size_t plane) {
  DCHECK_LT(plane, NumPlanes(dxgi_format));
  switch (dxgi_format) {
    // TODO(crbug.com/1011555): P010 formats are not fully supported by Skia.
    // Treat them the same as NV12 for the time being.
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
      // Y plane is accessed as R8 and UV plane is accessed as RG88 in D3D.
      return plane == 0 ? viz::RED_8 : viz::RG_88;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return viz::BGRA_8888;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return viz::RGBA_1010102;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return viz::RGBA_F16;
    default:
      NOTREACHED();
      return viz::BGRA_8888;
  }
}

gfx::Size PlaneSize(DXGI_FORMAT dxgi_format,
                    const gfx::Size& size,
                    size_t plane) {
  DCHECK_LT(plane, NumPlanes(dxgi_format));
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
      // Y plane is full size and UV plane is accessed as half size in D3D.
      return plane == 0 ? size : gfx::Size(size.width() / 2, size.height() / 2);
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return size;
    default:
      NOTREACHED();
      return gfx::Size();
  }
}

class ScopedRestoreTexture {
 public:
  ScopedRestoreTexture(gl::GLApi* api, GLenum target)
      : api_(api), target_(target) {
    DCHECK(target == GL_TEXTURE_2D || target == GL_TEXTURE_EXTERNAL_OES);
    GLint binding = 0;
    api->glGetIntegervFn(target == GL_TEXTURE_2D
                             ? GL_TEXTURE_BINDING_2D
                             : GL_TEXTURE_BINDING_EXTERNAL_OES,
                         &binding);
    prev_binding_ = binding;
  }

  ScopedRestoreTexture(const ScopedRestoreTexture&) = delete;
  ScopedRestoreTexture& operator=(const ScopedRestoreTexture&) = delete;

  ~ScopedRestoreTexture() { api_->glBindTextureFn(target_, prev_binding_); }

 private:
  const raw_ptr<gl::GLApi> api_;
  const GLenum target_;
  GLuint prev_binding_ = 0;
};

scoped_refptr<gles2::TexturePassthrough> CreateGLTexture(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
    GLenum texture_target = GL_TEXTURE_2D,
    unsigned array_slice = 0u,
    unsigned plane_index = 0u) {
  gl::GLApi* const api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, texture_target);

  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(texture_target, service_id);

  // These need to be set for the texture to be considered mipmap complete.
  api->glTexParameteriFn(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // These are not strictly required but guard against some checks if NPOT
  // texture support is disabled.
  api->glTexParameteriFn(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // The GL internal format can differ from the underlying swap chain or texture
  // format e.g. RGBA or RGB instead of BGRA or RED/RG for NV12 texture planes.
  // See EGL_ANGLE_d3d_texture_client_buffer spec for format restrictions.
  const auto internal_format = viz::GLInternalFormat(format);
  const auto data_type = viz::GLDataType(format);
  auto image = base::MakeRefCounted<gl::GLImageD3D>(
      size, internal_format, data_type, color_space, d3d11_texture, array_slice,
      plane_index, swap_chain);
  DCHECK_EQ(image->GetDataFormat(), viz::GLDataFormat(format));
  if (!image->Initialize()) {
    DLOG(ERROR) << "GLImageD3D::Initialize failed";
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }
  if (!image->BindTexImage(texture_target)) {
    DLOG(ERROR) << "GLImageD3D::BindTexImage failed";
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }

  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, texture_target);
  texture->SetLevelImage(texture_target, 0, image.get());
  GLint texture_memory_size = 0;
  api->glGetTexParameterivFn(texture_target, GL_MEMORY_SIZE_ANGLE,
                             &texture_memory_size);
  texture->SetEstimatedSize(texture_memory_size);

  return texture;
}

void CopyPlane(const uint8_t* source_memory,
               size_t source_stride,
               uint8_t* dest_memory,
               size_t dest_stride,
               viz::ResourceFormat format,
               const gfx::Size& size) {
  int row_bytes = size.width() * viz::BitsPerPixel(format) / 8;
  libyuv::CopyPlane(source_memory, source_stride, dest_memory, dest_stride,
                    row_bytes, size.height());
}

}  // namespace

// static
std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromSwapChainBuffer(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_back_buffer) {
  auto gl_texture =
      CreateGLTexture(format, size, color_space, d3d11_texture, swap_chain);
  if (!gl_texture) {
    DLOG(ERROR) << "Failed to create GL texture";
    return nullptr;
  }
  return base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture),
      /*dxgi_shared_handle_state=*/{}, /*shared_memory_handle=*/{},
      std::move(swap_chain), is_back_buffer));
}

// static
std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromDXGISharedHandle(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state) {
  DCHECK(dxgi_shared_handle_state);

  const bool has_webgpu_usage = !!(usage & SHARED_IMAGE_USAGE_WEBGPU);
  // Keyed mutexes are required for Dawn interop but are not used for XR
  // composition where fences are used instead.
  DCHECK(!has_webgpu_usage || dxgi_shared_handle_state->has_keyed_mutex());

  // Do not cache a GL texture in the backing if it could be owned by WebGPU
  // since there's no GL context to MakeCurrent in the destructor.
  scoped_refptr<gles2::TexturePassthrough> gl_texture;
  if (!has_webgpu_usage) {
    // Creating the GL texture doesn't require exclusive access to the
    // underlying D3D11 texture.
    gl_texture = CreateGLTexture(format, size, color_space, d3d11_texture);
    if (!gl_texture) {
      DLOG(ERROR) << "Failed to create GL texture";
      return nullptr;
    }
  }
  auto backing = base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture),
      std::move(dxgi_shared_handle_state)));
  return backing;
}

std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromGLTexture(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<gles2::TexturePassthrough> gl_texture) {
  return base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture)));
}

// static
std::vector<std::unique_ptr<SharedImageBacking>>
SharedImageBackingD3D::CreateFromVideoTexture(
    base::span<const Mailbox> mailboxes,
    DXGI_FORMAT dxgi_format,
    const gfx::Size& size,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    unsigned array_slice,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state) {
  DCHECK(d3d11_texture);
  DCHECK(SupportsVideoFormat(dxgi_format));
  DCHECK_EQ(mailboxes.size(), NumPlanes(dxgi_format));

  // Shared handle and keyed mutex are required for Dawn interop.
  const bool has_webgpu_usage = usage & gpu::SHARED_IMAGE_USAGE_WEBGPU;
  const bool has_keyed_mutex =
      dxgi_shared_handle_state && dxgi_shared_handle_state->has_keyed_mutex();
  DCHECK(!has_webgpu_usage || has_keyed_mutex);

  std::vector<std::unique_ptr<SharedImageBacking>> shared_images(
      NumPlanes(dxgi_format));
  for (size_t plane_index = 0; plane_index < shared_images.size();
       plane_index++) {
    const auto& mailbox = mailboxes[plane_index];

    const auto plane_format = PlaneFormat(dxgi_format, plane_index);
    const auto plane_size = PlaneSize(dxgi_format, size, plane_index);

    // Shared image does not need to store the colorspace since it is already
    // stored on the VideoFrame which is provided upon presenting the overlay.
    // To prevent the developer from mistakenly using it, provide the invalid
    // value from default-construction.
    constexpr gfx::ColorSpace kInvalidColorSpace;

    auto gl_texture = CreateGLTexture(
        plane_format, plane_size, kInvalidColorSpace, d3d11_texture,
        /*swap_chain=*/nullptr, GL_TEXTURE_EXTERNAL_OES, array_slice,
        plane_index);
    if (!gl_texture) {
      DLOG(ERROR) << "Failed to create GL texture";
      return {};
    }

    shared_images[plane_index] = base::WrapUnique(new SharedImageBackingD3D(
        mailbox, plane_format, plane_size, kInvalidColorSpace,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, d3d11_texture,
        std::move(gl_texture), dxgi_shared_handle_state));
    shared_images[plane_index]->SetCleared();
  }

  return shared_images;
}

// static
std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromSharedMemoryHandle(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    gfx::GpuMemoryBufferHandle shared_memory_handle) {
  DCHECK_EQ(shared_memory_handle.type, gfx::SHARED_MEMORY_BUFFER);
  auto gl_texture = CreateGLTexture(format, size, color_space, d3d11_texture);
  if (!gl_texture) {
    DLOG(ERROR) << "Failed to create GL texture";
    return nullptr;
  }
  auto backing = base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture),
      /*dxgi_shared_handle_state=*/{}, std::move(shared_memory_handle)));
  return backing;
}

SharedImageBackingD3D::SharedImageBackingD3D(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<gles2::TexturePassthrough> gl_texture,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state,
    gfx::GpuMemoryBufferHandle shared_memory_handle,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_back_buffer)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          gl_texture
              ? gl_texture->estimated_size()
              : gfx::BufferSizeForBufferFormat(size, viz::BufferFormat(format)),
          false /* is_thread_safe */),
      d3d11_texture_(std::move(d3d11_texture)),
      gl_texture_(std::move(gl_texture)),
      dxgi_shared_handle_state_(std::move(dxgi_shared_handle_state)),
      shared_memory_handle_(std::move(shared_memory_handle)),
      swap_chain_(std::move(swap_chain)),
      is_back_buffer_(is_back_buffer) {
  const bool has_webgpu_usage = !!(usage & SHARED_IMAGE_USAGE_WEBGPU);
  DCHECK(has_webgpu_usage || gl_texture_);
}

SharedImageBackingD3D::~SharedImageBackingD3D() {
  if (!have_context())
    gl_texture_->MarkContextLost();
  gl_texture_.reset();
  dxgi_shared_handle_state_.reset();
  swap_chain_.Reset();
  d3d11_texture_.Reset();

#if BUILDFLAG(USE_DAWN)
  external_image_ = nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

ID3D11Texture2D* SharedImageBackingD3D::GetOrCreateStagingTexture() {
  if (!staging_texture_) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    DCHECK(d3d11_texture_);
    d3d11_texture_->GetDevice(&d3d11_device);

    D3D11_TEXTURE2D_DESC texture_desc;
    d3d11_texture_->GetDesc(&texture_desc);

    D3D11_TEXTURE2D_DESC staging_desc = {};
    staging_desc.Width = texture_desc.Width;
    staging_desc.Height = texture_desc.Height;
    staging_desc.Format = texture_desc.Format;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags =
        D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = d3d11_device->CreateTexture2D(&staging_desc, nullptr,
                                               &staging_texture_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create staging texture. hr=" << std::hex << hr;
      return nullptr;
    }

    constexpr char kStagingTextureLabel[] = "SharedImageD3D_StagingTexture";
    // Add debug label to the long lived texture.
    staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName,
                                     strlen(kStagingTextureLabel),
                                     kStagingTextureLabel);
  }
  return staging_texture_.Get();
}

void SharedImageBackingD3D::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
  if (!shared_memory_handle_.is_null())
    needs_upload_to_gpu_ = true;
}

bool SharedImageBackingD3D::UploadToGpuIfNeeded() {
  if (!needs_upload_to_gpu_)
    return true;

  gpu::SharedMemoryRegionWrapper mapped_shared_memory;
  mapped_shared_memory.Initialize(shared_memory_handle_, size(), format());

  if (!mapped_shared_memory.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory";
    return false;
  }

  const uint8_t* source_memory = mapped_shared_memory.GetMemory();
  const size_t source_stride = mapped_shared_memory.GetStride();

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  DCHECK(d3d11_texture_);
  d3d11_texture_->GetDevice(&d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  D3D11_TEXTURE2D_DESC texture_desc;
  d3d11_texture_->GetDesc(&texture_desc);

  if (texture_desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
    Microsoft::WRL::ComPtr<ID3D11Device3> device3;
    HRESULT hr = d3d11_device.As(&device3);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to retrieve ID3D11Device3. hr=" << std::hex << hr;
      return false;
    }
    hr = device_context->Map(d3d11_texture_.Get(), 0, D3D11_MAP_WRITE, 0,
                             nullptr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to map texture for write. hr = " << std::hex << hr;
      return false;
    }
    device3->WriteToSubresource(d3d11_texture_.Get(), 0, nullptr, source_memory,
                                source_stride, 0);
    device_context->Unmap(d3d11_texture_.Get(), 0);
  } else {
    ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();
    if (!staging_texture)
      return false;
    D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
    HRESULT hr = device_context->Map(staging_texture, 0, D3D11_MAP_WRITE, 0,
                                     &mapped_resource);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to map texture for write. hr=" << std::hex << hr;
      return false;
    }
    uint8_t* dest_memory = static_cast<uint8_t*>(mapped_resource.pData);
    const size_t dest_stride = mapped_resource.RowPitch;
    CopyPlane(source_memory, source_stride, dest_memory, dest_stride, format(),
              size());
    device_context->Unmap(staging_texture, 0);
    device_context->CopySubresourceRegion(d3d11_texture_.Get(), 0, 0, 0, 0,
                                          staging_texture, 0, nullptr);
  }
  needs_upload_to_gpu_ = false;
  return true;
}

bool SharedImageBackingD3D::CopyToGpuMemoryBuffer() {
  if (shared_memory_handle_.is_null()) {
    DLOG(ERROR)
        << "Called CopyToGpuMemoryBuffer for backing without shared memory GMB";
    return false;
  }

  gpu::SharedMemoryRegionWrapper mapped_shared_memory;
  mapped_shared_memory.Initialize(shared_memory_handle_, size(), format());

  if (!mapped_shared_memory.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory";
    return false;
  }

  uint8_t* dest_memory = mapped_shared_memory.GetMemory();
  const size_t dest_stride = mapped_shared_memory.GetStride();

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  DCHECK(d3d11_texture_);
  d3d11_texture_->GetDevice(&d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  D3D11_TEXTURE2D_DESC texture_desc;
  d3d11_texture_->GetDesc(&texture_desc);

  if (texture_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
    Microsoft::WRL::ComPtr<ID3D11Device3> device3;
    HRESULT hr = d3d11_device.As(&device3);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to retrieve ID3D11Device3. hr=" << std::hex << hr;
      return false;
    }
    hr = device_context->Map(d3d11_texture_.Get(), 0, D3D11_MAP_READ, 0,
                             nullptr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to map texture for read. hr=" << std::hex << hr;
      return false;
    }
    device3->ReadFromSubresource(dest_memory, dest_stride, 0,
                                 d3d11_texture_.Get(), 0, nullptr);
    device_context->Unmap(d3d11_texture_.Get(), 0);
  } else {
    ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();
    if (!staging_texture)
      return false;
    device_context->CopySubresourceRegion(staging_texture, 0, 0, 0, 0,
                                          d3d11_texture_.Get(), 0, nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
    HRESULT hr = device_context->Map(staging_texture, 0, D3D11_MAP_READ, 0,
                                     &mapped_resource);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to map texture for read. hr=" << std::hex << hr;
      return false;
    }
    const uint8_t* source_memory = static_cast<uint8_t*>(mapped_resource.pData);
    const size_t source_stride = mapped_resource.RowPitch;
    CopyPlane(source_memory, source_stride, dest_memory, dest_stride, format(),
              size());
    device_context->Unmap(staging_texture, 0);
  }
  return true;
}

bool SharedImageBackingD3D::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  mailbox_manager->ProduceTexture(mailbox(), gl_texture_.get());
  return true;
}

uint32_t SharedImageBackingD3D::GetAllowedDawnUsages() const {
  // TODO(crbug.com/2709243): Figure out other SI flags, if any.
  DCHECK(usage() & gpu::SHARED_IMAGE_USAGE_WEBGPU);
  return static_cast<uint32_t>(
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment);
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingD3D::ProduceDawn(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker,
                                   WGPUDevice device,
                                   WGPUBackendType backend_type) {
#if BUILDFLAG(USE_DAWN)
  const viz::ResourceFormat viz_resource_format = format();
  const WGPUTextureFormat wgpu_format = viz::ToWGPUFormat(viz_resource_format);
  if (wgpu_format == WGPUTextureFormat_Undefined) {
    DLOG(ERROR) << "Unsupported viz format found: " << viz_resource_format;
    return nullptr;
  }

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.format = wgpu_format;
  texture_descriptor.usage = GetAllowedDawnUsages();
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  // We need to have an internal usage of CopySrc in order to use
  // CopyTextureToTextureInternal.
  WGPUDawnTextureInternalUsageDescriptor internalDesc = {};
  internalDesc.chain.sType = WGPUSType_DawnTextureInternalUsageDescriptor;
  internalDesc.internalUsage = WGPUTextureUsage_CopySrc;
  texture_descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&internalDesc);

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == WGPUBackendType_OpenGLES) {
    // EGLImage textures do not support sampling, at the moment.
    texture_descriptor.usage &= ~WGPUTextureUsage_TextureBinding;
    EGLImage egl_image =
        static_cast<gl::GLImageD3D*>(GetGLImage())->egl_image();
    if (!egl_image) {
      DLOG(ERROR) << "Failed to create EGLImage";
      return nullptr;
    }
    return std::make_unique<SharedImageRepresentationDawnEGLImage>(
        manager, this, tracker, device, egl_image, texture_descriptor);
  }
#endif

  // Persistently open the shared handle by caching it on this backing.
  if (!external_image_) {
    DCHECK(dxgi_shared_handle_state_);
    const HANDLE shared_handle = dxgi_shared_handle_state_->GetSharedHandle();
    DCHECK(base::win::HandleTraits::IsHandleValid(shared_handle));

    dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle
        externalImageDesc;
    externalImageDesc.cTextureDescriptor = &texture_descriptor;
    externalImageDesc.sharedHandle = shared_handle;

    external_image_ = dawn_native::d3d12::ExternalImageDXGI::Create(
        device, &externalImageDesc);

    if (!external_image_) {
      DLOG(ERROR) << "Failed to create external image";
      return nullptr;
    }
  }

  return std::make_unique<SharedImageRepresentationDawnD3D>(
      manager, this, tracker, device, external_image_.get());
#else
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

void SharedImageBackingD3D::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  base::trace_event::MemoryAllocatorDumpGuid service_guid =
      gl::GetGLTextureServiceGUIDForTracing(gl_texture_->service_id());
  pmd->CreateSharedGlobalAllocatorDump(service_guid);

  int importance = 2;  // This client always owns the ref.
  pmd->AddOwnershipEdge(client_guid, service_guid, importance);

  // Swap chain textures only have one level backed by an image.
  GetGLImage()->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

bool SharedImageBackingD3D::BeginAccessD3D12() {
  if (dxgi_shared_handle_state_)
    return dxgi_shared_handle_state_->BeginAccessD3D12();
  // D3D12 access is only allowed with shared handle and keyed mutex.
  return false;
}

void SharedImageBackingD3D::EndAccessD3D12() {
  if (dxgi_shared_handle_state_)
    dxgi_shared_handle_state_->EndAccessD3D12();
}

bool SharedImageBackingD3D::BeginAccessD3D11() {
  if (dxgi_shared_handle_state_)
    return dxgi_shared_handle_state_->BeginAccessD3D11();
  // D3D11 access is allowed without shared handle and keyed mutex.
  return true;
}

void SharedImageBackingD3D::EndAccessD3D11() {
  if (dxgi_shared_handle_state_)
    dxgi_shared_handle_state_->EndAccessD3D11();
}

gl::GLImage* SharedImageBackingD3D::GetGLImage() const {
  return gl_texture_->GetLevelImage(gl_texture_->target(), 0u);
}

bool SharedImageBackingD3D::PresentSwapChain() {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain");
  if (!swap_chain_ || !is_back_buffer_) {
    DLOG(ERROR) << "Backing does not correspond to back buffer of swap chain";
    return false;
  }

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;

  UINT flags = DXGI_PRESENT_ALLOW_TEARING;

  HRESULT hr = swap_chain_->Present1(0 /* interval */, flags, &params);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return false;
  }

  gl::GLApi* const api = gl::g_current_gl_context;

  DCHECK_EQ(gl_texture_->target(), static_cast<unsigned>(GL_TEXTURE_2D));
  ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);

  api->glBindTextureFn(GL_TEXTURE_2D, gl_texture_->service_id());
  if (!GetGLImage()->BindTexImage(GL_TEXTURE_2D)) {
    DLOG(ERROR) << "GLImage::BindTexImage failed";
    return false;
  }

  TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain::Flush");
  // Flush device context through ANGLE otherwise present could be deferred.
  api->glFlushFn();
  return true;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingD3D::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceGLTexturePassthrough");
  if (!UploadToGpuIfNeeded()) {
    DLOG(ERROR) << "UploadToGpuIfNeeded failed";
    return nullptr;
  }
  // Lazily create a GL texture if it wasn't provided on initialization.
  auto gl_texture = gl_texture_;
  if (!gl_texture) {
    gl_texture =
        CreateGLTexture(format(), size(), color_space(), d3d11_texture_);
    if (!gl_texture) {
      DLOG(ERROR) << "Failed to create GL texture";
      return nullptr;
    }
  }
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughD3D>(
      manager, this, tracker, std::move(gl_texture));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingD3D::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return SharedImageRepresentationSkiaGL::Create(
      ProduceGLTexturePassthrough(manager, tracker), std::move(context_state),
      manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingD3D::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceOverlay");
  // Prefer GLImageMemory for shared memory case so that we don't upload to a
  // texture if it ends up in an overlay.
  if (!shared_memory_handle_.is_null()) {
    auto gl_image = base::MakeRefCounted<gl::GLImageSharedMemory>(size());
    if (!gl_image->Initialize(
            shared_memory_handle_.region, shared_memory_handle_.id,
            viz::BufferFormat(format()), shared_memory_handle_.offset,
            shared_memory_handle_.stride)) {
      DLOG(ERROR) << "Failed to initialize GLImageSharedMemory";
      return nullptr;
    }
    return std::make_unique<SharedImageRepresentationOverlayD3D>(
        manager, this, tracker, std::move(gl_image));
  }
  return std::make_unique<SharedImageRepresentationOverlayD3D>(
      manager, this, tracker, GetGLImage());
}

}  // namespace gpu
