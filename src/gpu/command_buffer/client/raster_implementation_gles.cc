// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"  // nogncheck
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gl_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace gpu {
namespace raster {

namespace {

GLenum SkColorTypeToGLDataFormat(SkColorType color_type) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
      return GL_RGBA;
    case kBGRA_8888_SkColorType:
      return GL_BGRA_EXT;
    default:
      DLOG(ERROR) << "Unknown SkColorType " << color_type;
  }
  NOTREACHED();
  return 0;
}

GLenum SkColorTypeToGLDataType(SkColorType color_type) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
      return GL_UNSIGNED_BYTE;
    default:
      DLOG(ERROR) << "Unknown SkColorType " << color_type;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    ContextSupport* context_support)
    : gl_(gl), context_support_(context_support) {}

RasterImplementationGLES::~RasterImplementationGLES() = default;

void RasterImplementationGLES::Finish() {
  gl_->Finish();
}

void RasterImplementationGLES::Flush() {
  gl_->Flush();
}

void RasterImplementationGLES::ShallowFlushCHROMIUM() {
  gl_->ShallowFlushCHROMIUM();
}

void RasterImplementationGLES::OrderingBarrierCHROMIUM() {
  gl_->OrderingBarrierCHROMIUM();
}

GLenum RasterImplementationGLES::GetError() {
  return gl_->GetError();
}

GLenum RasterImplementationGLES::GetGraphicsResetStatusKHR() {
  return gl_->GetGraphicsResetStatusKHR();
}

void RasterImplementationGLES::LoseContextCHROMIUM(GLenum current,
                                                   GLenum other) {
  gl_->LoseContextCHROMIUM(current, other);
}

void RasterImplementationGLES::GenQueriesEXT(GLsizei n, GLuint* queries) {
  gl_->GenQueriesEXT(n, queries);
}

void RasterImplementationGLES::DeleteQueriesEXT(GLsizei n,
                                                const GLuint* queries) {
  gl_->DeleteQueriesEXT(n, queries);
}

void RasterImplementationGLES::BeginQueryEXT(GLenum target, GLuint id) {
  gl_->BeginQueryEXT(target, id);
}

void RasterImplementationGLES::EndQueryEXT(GLenum target) {
  gl_->EndQueryEXT(target);
}

void RasterImplementationGLES::QueryCounterEXT(GLuint id, GLenum target) {
  gl_->QueryCounterEXT(id, target);
}

void RasterImplementationGLES::GetQueryObjectuivEXT(GLuint id,
                                                    GLenum pname,
                                                    GLuint* params) {
  gl_->GetQueryObjectuivEXT(id, pname, params);
}

void RasterImplementationGLES::GetQueryObjectui64vEXT(GLuint id,
                                                      GLenum pname,
                                                      GLuint64* params) {
  gl_->GetQueryObjectui64vEXT(id, pname, params);
}

void RasterImplementationGLES::CopySubTexture(
    const gpu::Mailbox& source_mailbox,
    const gpu::Mailbox& dest_mailbox,
    GLenum dest_target,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha) {
  GLuint texture_ids[2] = {
      CreateAndConsumeForGpuRaster(source_mailbox),
      CreateAndConsumeForGpuRaster(dest_mailbox),
  };
  DCHECK(texture_ids[0]);
  DCHECK(texture_ids[1]);

  BeginSharedImageAccessDirectCHROMIUM(
      texture_ids[0], GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  BeginSharedImageAccessDirectCHROMIUM(
      texture_ids[1], GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  gl_->CopySubTextureCHROMIUM(texture_ids[0], 0, dest_target, texture_ids[1], 0,
                              xoffset, yoffset, x, y, width, height,
                              unpack_flip_y, unpack_premultiply_alpha,
                              false /* upack_unmultiply_alpha */);

  EndSharedImageAccessDirectCHROMIUM(texture_ids[0]);
  EndSharedImageAccessDirectCHROMIUM(texture_ids[1]);
  gl_->DeleteTextures(2, texture_ids);
}

void RasterImplementationGLES::WritePixels(const gpu::Mailbox& dest_mailbox,
                                           int dst_x_offset,
                                           int dst_y_offset,
                                           GLenum texture_target,
                                           GLuint row_bytes,
                                           const SkImageInfo& src_info,
                                           const void* src_pixels) {
  DCHECK_EQ(row_bytes, src_info.minRowBytes());
  GLuint texture_id = CreateAndConsumeForGpuRaster(dest_mailbox);
  BeginSharedImageAccessDirectCHROMIUM(
      texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  gl_->BindTexture(texture_target, texture_id);
  gl_->TexSubImage2D(texture_target, 0, dst_x_offset, dst_y_offset,
                     src_info.width(), src_info.height(),
                     SkColorTypeToGLDataFormat(src_info.colorType()),
                     SkColorTypeToGLDataType(src_info.colorType()), src_pixels);
  gl_->BindTexture(texture_target, 0);

  EndSharedImageAccessDirectCHROMIUM(texture_id);
  DeleteGpuRasterTexture(texture_id);
}

void RasterImplementationGLES::ConvertYUVMailboxesToRGB(
    const gpu::Mailbox& dest_mailbox,
    SkYUVColorSpace planes_yuv_color_space,
    const gpu::Mailbox& y_plane_mailbox,
    const gpu::Mailbox& u_plane_mailbox,
    const gpu::Mailbox& v_plane_mailbox) {
  NOTREACHED();
}

void RasterImplementationGLES::ConvertNV12MailboxesToRGB(
    const gpu::Mailbox& dest_mailbox,
    SkYUVColorSpace planes_yuv_color_space,
    const gpu::Mailbox& y_plane_mailbox,
    const gpu::Mailbox& uv_planes_mailbox) {
  NOTREACHED();
}

void RasterImplementationGLES::BeginRasterCHROMIUM(
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    const gfx::ColorSpace& color_space,
    const GLbyte* mailbox) {
  NOTREACHED();
}

void RasterImplementationGLES::RasterCHROMIUM(
    const cc::DisplayItemList* list,
    cc::ImageProvider* provider,
    const gfx::Size& content_size,
    const gfx::Rect& full_raster_rect,
    const gfx::Rect& playback_rect,
    const gfx::Vector2dF& post_translate,
    GLfloat post_scale,
    bool requires_clear,
    size_t* max_op_size_hint) {
  NOTREACHED();
}

void RasterImplementationGLES::SetActiveURLCHROMIUM(const char* url) {
  gl_->SetActiveURLCHROMIUM(url);
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  NOTREACHED();
}

SyncToken RasterImplementationGLES::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    uint32_t transfer_cache_entry_id,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  NOTREACHED();
  return SyncToken();
}

void RasterImplementationGLES::ReadbackARGBPixelsAsync(
    const gpu::Mailbox& source_mailbox,
    GLenum source_target,
    const gfx::Size& dst_size,
    unsigned char* out,
    GLenum format,
    base::OnceCallback<void(bool)> readback_done) {
  DCHECK(!readback_done.is_null());

  GLuint texture_id = CreateAndConsumeForGpuRaster(source_mailbox);
  BeginSharedImageAccessDirectCHROMIUM(
      texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  GetGLHelper()->ReadbackTextureAsync(
      texture_id, source_target, dst_size, out, format,
      base::BindOnce(&RasterImplementationGLES::OnReadARGBPixelsAsync,
                     weak_ptr_factory_.GetWeakPtr(), texture_id,
                     std::move(readback_done)));
}

void RasterImplementationGLES::OnReadARGBPixelsAsync(
    GLuint texture_id,
    base::OnceCallback<void(bool)> readback_done,
    bool success) {
  DCHECK(texture_id);
  EndSharedImageAccessDirectCHROMIUM(texture_id);
  DeleteGpuRasterTexture(texture_id);

  std::move(readback_done).Run(success);
}

void RasterImplementationGLES::ReadbackYUVPixelsAsync(
    const gpu::Mailbox& source_mailbox,
    GLenum source_target,
    const gfx::Size& source_size,
    const gfx::Rect& output_rect,
    bool vertically_flip_texture,
    int y_plane_row_stride_bytes,
    unsigned char* y_plane_data,
    int u_plane_row_stride_bytes,
    unsigned char* u_plane_data,
    int v_plane_row_stride_bytes,
    unsigned char* v_plane_data,
    const gfx::Point& paste_location,
    base::OnceCallback<void()> release_mailbox,
    base::OnceCallback<void(bool)> readback_done) {
  GLuint shared_texture_id = CreateAndConsumeForGpuRaster(source_mailbox);
  BeginSharedImageAccessDirectCHROMIUM(
      shared_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  base::OnceCallback<void()> on_release_mailbox =
      base::BindOnce(&RasterImplementationGLES::OnReleaseMailbox,
                     weak_ptr_factory_.GetWeakPtr(), shared_texture_id,
                     std::move(release_mailbox));

  // The YUV readback path only works for 2D textures.
  GLuint texture_for_readback = shared_texture_id;
  GLuint copy_texture_id = 0;
  if (source_target != GL_TEXTURE_2D) {
    int width = source_size.width();
    int height = source_size.height();

    gl_->GenTextures(1, &copy_texture_id);
    gl_->BindTexture(GL_TEXTURE_2D, copy_texture_id);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, nullptr);
    gl_->CopyTextureCHROMIUM(shared_texture_id, 0, GL_TEXTURE_2D,
                             copy_texture_id, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
                             0, 0);
    texture_for_readback = copy_texture_id;

    // |copy_texture_id| now contains the texture we want to copy, release the
    // pinned mailbox.
    std::move(on_release_mailbox).Run();
  }

  DCHECK(GetGLHelper());
  gpu::ReadbackYUVInterface* const yuv_reader =
      GetGLHelper()->GetReadbackPipelineYUV(vertically_flip_texture);
  yuv_reader->ReadbackYUV(
      texture_for_readback, source_size, gfx::Rect(source_size),
      y_plane_row_stride_bytes, y_plane_data, u_plane_row_stride_bytes,
      u_plane_data, v_plane_row_stride_bytes, v_plane_data, paste_location,
      base::BindOnce(&RasterImplementationGLES::OnReadYUVPixelsAsync,
                     weak_ptr_factory_.GetWeakPtr(), copy_texture_id,
                     std::move(on_release_mailbox), std::move(readback_done)));
}

void RasterImplementationGLES::OnReadYUVPixelsAsync(
    GLuint copy_texture_id,
    base::OnceCallback<void()> on_release_mailbox,
    base::OnceCallback<void(bool)> readback_done,
    bool success) {
  if (copy_texture_id) {
    DCHECK(on_release_mailbox.is_null());
    gl_->DeleteTextures(1, &copy_texture_id);
  } else {
    DCHECK(!on_release_mailbox.is_null());
    std::move(on_release_mailbox).Run();
  }

  std::move(readback_done).Run(success);
}

void RasterImplementationGLES::OnReleaseMailbox(
    GLuint shared_texture_id,
    base::OnceCallback<void()> release_mailbox) {
  DCHECK(shared_texture_id);
  DCHECK(!release_mailbox.is_null());

  EndSharedImageAccessDirectCHROMIUM(shared_texture_id);
  DeleteGpuRasterTexture(shared_texture_id);
  std::move(release_mailbox).Run();
}

GLuint RasterImplementationGLES::CreateAndConsumeForGpuRaster(
    const gpu::Mailbox& mailbox) {
  return mailbox.IsSharedImage()
             ? gl_->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name)
             : gl_->CreateAndConsumeTextureCHROMIUM(mailbox.name);
}

void RasterImplementationGLES::DeleteGpuRasterTexture(GLuint texture) {
  gl_->DeleteTextures(1u, &texture);
}

void RasterImplementationGLES::BeginGpuRaster() {
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceBeginCHROMIUM("BeginGpuRaster", "GpuRasterization");
}

void RasterImplementationGLES::EndGpuRaster() {
  // Restore default GL unpack alignment.  TextureUploader expects this.
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceEndCHROMIUM();

  // Reset cached raster state.
  gl_->ActiveTexture(GL_TEXTURE0);
}

void RasterImplementationGLES::BeginSharedImageAccessDirectCHROMIUM(
    GLuint texture,
    GLenum mode) {
  gl_->BeginSharedImageAccessDirectCHROMIUM(texture, mode);
}

void RasterImplementationGLES::EndSharedImageAccessDirectCHROMIUM(
    GLuint texture) {
  gl_->EndSharedImageAccessDirectCHROMIUM(texture);
}

void RasterImplementationGLES::InitializeDiscardableTextureCHROMIUM(
    GLuint texture) {
  gl_->InitializeDiscardableTextureCHROMIUM(texture);
}

void RasterImplementationGLES::UnlockDiscardableTextureCHROMIUM(
    GLuint texture) {
  gl_->UnlockDiscardableTextureCHROMIUM(texture);
}

bool RasterImplementationGLES::LockDiscardableTextureCHROMIUM(GLuint texture) {
  return gl_->LockDiscardableTextureCHROMIUM(texture);
}

void RasterImplementationGLES::TraceBeginCHROMIUM(const char* category_name,
                                                  const char* trace_name) {
  gl_->TraceBeginCHROMIUM(category_name, trace_name);
}

void RasterImplementationGLES::TraceEndCHROMIUM() {
  gl_->TraceEndCHROMIUM();
}

// InterfaceBase implementation.
void RasterImplementationGLES::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  gl_->GenSyncTokenCHROMIUM(sync_token);
}
void RasterImplementationGLES::GenUnverifiedSyncTokenCHROMIUM(
    GLbyte* sync_token) {
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token);
}
void RasterImplementationGLES::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                        GLsizei count) {
  gl_->VerifySyncTokensCHROMIUM(sync_tokens, count);
}
void RasterImplementationGLES::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  gl_->WaitSyncTokenCHROMIUM(sync_token);
}

GLHelper* RasterImplementationGLES::GetGLHelper() {
  if (!gl_helper_) {
    DCHECK(gl_);
    DCHECK(context_support_);
    gl_helper_ = std::make_unique<GLHelper>(gl_, context_support_);
  }

  return gl_helper_.get();
}

}  // namespace raster
}  // namespace gpu
