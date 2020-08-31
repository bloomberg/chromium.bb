// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/paint_canvas_video_renderer.h"

#include <GLES3/gl3.h>
#include <limits>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "media/renderers/yuv_util.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it is ARGB. Commented out lines below don't exist in libyuv yet and are
// shown here to indicate where ideal conversions are currently missing.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I400_TO_ARGB libyuv::I400ToARGB
#define LIBYUV_I420_TO_ARGB libyuv::I420ToARGB
#define LIBYUV_I422_TO_ARGB libyuv::I422ToARGB
#define LIBYUV_I444_TO_ARGB libyuv::I444ToARGB

#define LIBYUV_I420ALPHA_TO_ARGB libyuv::I420AlphaToARGB

#define LIBYUV_J400_TO_ARGB libyuv::J400ToARGB
#define LIBYUV_J420_TO_ARGB libyuv::J420ToARGB
#define LIBYUV_J422_TO_ARGB libyuv::J422ToARGB
#define LIBYUV_J444_TO_ARGB libyuv::J444ToARGB

#define LIBYUV_H420_TO_ARGB libyuv::H420ToARGB
#define LIBYUV_H422_TO_ARGB libyuv::H422ToARGB
#define LIBYUV_H444_TO_ARGB libyuv::H444ToARGB

#define LIBYUV_U420_TO_ARGB libyuv::U420ToARGB
#define LIBYUV_U422_TO_ARGB libyuv::U422ToARGB
#define LIBYUV_U444_TO_ARGB libyuv::U444ToARGB

#define LIBYUV_I010_TO_ARGB libyuv::I010ToARGB
#define LIBYUV_I210_TO_ARGB libyuv::I210ToARGB
// #define LIBYUV_I410_TO_ARGB libyuv::I410ToARGB

// #define LIBYUV_J010_TO_ARGB libyuv::J010ToARGB
// #define LIBYUV_J210_TO_ARGB libyuv::J210ToARGB
// #define LIBYUV_J410_TO_ARGB libyuv::J410ToARGB

#define LIBYUV_H010_TO_ARGB libyuv::H010ToARGB
#define LIBYUV_H210_TO_ARGB libyuv::H210ToARGB
// #define LIBYUV_H410_TO_ARGB libyuv::H410ToARGB

#define LIBYUV_U010_TO_ARGB libyuv::U010ToARGB
#define LIBYUV_U210_TO_ARGB libyuv::U210ToARGB
// #define LIBYUV_U410_TO_ARGB libyuv::U410ToARGB

#define LIBYUV_NV12_TO_ARGB libyuv::NV12ToARGB
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I400_TO_ARGB libyuv::I400ToARGB
#define LIBYUV_I420_TO_ARGB libyuv::I420ToABGR
#define LIBYUV_I422_TO_ARGB libyuv::I422ToABGR
#define LIBYUV_I444_TO_ARGB libyuv::I444ToABGR

#define LIBYUV_I420ALPHA_TO_ARGB libyuv::I420AlphaToABGR

#define LIBYUV_J400_TO_ARGB libyuv::J400ToARGB
#define LIBYUV_J420_TO_ARGB libyuv::J420ToABGR
#define LIBYUV_J422_TO_ARGB libyuv::J422ToABGR
#define LIBYUV_J444_TO_ARGB libyuv::J444ToABGR

#define LIBYUV_H420_TO_ARGB libyuv::H420ToABGR
#define LIBYUV_H422_TO_ARGB libyuv::H422ToABGR
#define LIBYUV_H444_TO_ARGB libyuv::H444ToABGR

#define LIBYUV_U420_TO_ARGB libyuv::U420ToABGR
#define LIBYUV_U422_TO_ARGB libyuv::U422ToABGR
#define LIBYUV_U444_TO_ARGB libyuv::U444ToABGR

#define LIBYUV_I010_TO_ARGB libyuv::I010ToABGR
#define LIBYUV_I210_TO_ARGB libyuv::I210ToABGR
// #define LIBYUV_I410_TO_ARGB libyuv::I410ToABGR

// #define LIBYUV_J010_TO_ARGB libyuv::J010ToABGR
// #define LIBYUV_J210_TO_ARGB libyuv::J210ToABGR
// #define LIBYUV_J410_TO_ARGB libyuv::J410ToABGR

#define LIBYUV_H010_TO_ARGB libyuv::H010ToABGR
#define LIBYUV_H210_TO_ARGB libyuv::H210ToABGR
// #define LIBYUV_H410_TO_ARGB libyuv::H410ToABGR

#define LIBYUV_U010_TO_ARGB libyuv::H010ToABGR
#define LIBYUV_U210_TO_ARGB libyuv::U210ToABGR
// #define LIBYUV_U410_TO_ARGB libyuv::U410ToABGR

#define LIBYUV_NV12_TO_ARGB libyuv::NV12ToABGR
#else
#error Unexpected Skia ARGB_8888 layout!
#endif

namespace media {

namespace {

// This class keeps the last image drawn.
// We delete the temporary resource if it is not used for 3 seconds.
const int kTemporaryResourceDeletionDelay = 3;  // Seconds;

class SyncTokenClientImpl : public VideoFrame::SyncTokenClient {
 public:
  explicit SyncTokenClientImpl(gpu::InterfaceBase* ib) : ib_(ib) {}
  ~SyncTokenClientImpl() override = default;
  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    ib_->GenSyncTokenCHROMIUM(sync_token->GetData());
  }
  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    ib_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

 private:
  gpu::InterfaceBase* ib_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncTokenClientImpl);
};

// Helper class that begins/ends access to a mailbox within a scope. The mailbox
// must have been imported into |texture|.
class ScopedSharedImageAccess {
 public:
  ScopedSharedImageAccess(
      gpu::gles2::GLES2Interface* gl,
      GLuint texture,
      const gpu::Mailbox& mailbox,
      GLenum access = GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)
      : gl(gl),
        ri(nullptr),
        texture(texture),
        is_shared_image(mailbox.IsSharedImage()) {
    if (is_shared_image)
      gl->BeginSharedImageAccessDirectCHROMIUM(texture, access);
  }

  // TODO(crbug.com/1023270): Remove this ctor once we're no longer relying on
  // texture ids for Mailbox access as that is only supported on
  // RasterImplementationGLES.
  ScopedSharedImageAccess(
      gpu::raster::RasterInterface* ri,
      GLuint texture,
      const gpu::Mailbox& mailbox,
      GLenum access = GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)
      : gl(nullptr),
        ri(ri),
        texture(texture),
        is_shared_image(mailbox.IsSharedImage()) {
    if (is_shared_image)
      ri->BeginSharedImageAccessDirectCHROMIUM(texture, access);
  }

  ~ScopedSharedImageAccess() {
    if (is_shared_image) {
      if (gl)
        gl->EndSharedImageAccessDirectCHROMIUM(texture);
      else
        ri->EndSharedImageAccessDirectCHROMIUM(texture);
    }
  }

 private:
  gpu::gles2::GLES2Interface* gl;
  gpu::raster::RasterInterface* ri;
  GLuint texture;
  bool is_shared_image;
};

// Waits for a sync token and import the mailbox as texture.
GLuint SynchronizeAndImportMailbox(gpu::gles2::GLES2Interface* gl,
                                   const gpu::SyncToken& sync_token,
                                   const gpu::Mailbox& mailbox) {
  gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  return mailbox.IsSharedImage()
             ? gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name)
             : gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);
}

// TODO(crbug.com/1023270): Remove this ctor once we're no longer relying on
// texture ids for Mailbox access as that is only supported on
// RasterImplementationGLES.
GLuint SynchronizeAndImportMailbox(gpu::raster::RasterInterface* ri,
                                   const gpu::SyncToken& sync_token,
                                   const gpu::Mailbox& mailbox) {
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  return ri->CreateAndConsumeForGpuRaster(mailbox);
}

const gpu::MailboxHolder& GetVideoFrameMailboxHolder(VideoFrame* video_frame) {
  DCHECK(video_frame->HasTextures());
  DCHECK_EQ(video_frame->NumTextures(), 1u);

  DCHECK(PIXEL_FORMAT_ARGB == video_frame->format() ||
         PIXEL_FORMAT_XRGB == video_frame->format() ||
         PIXEL_FORMAT_RGB24 == video_frame->format() ||
         PIXEL_FORMAT_ABGR == video_frame->format() ||
         PIXEL_FORMAT_XB30 == video_frame->format() ||
         PIXEL_FORMAT_XR30 == video_frame->format() ||
         PIXEL_FORMAT_NV12 == video_frame->format())
      << "Format: " << VideoPixelFormatToString(video_frame->format());

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
         mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
      << mailbox_holder.texture_target;
  return mailbox_holder;
}

// Imports a VideoFrame that contains a single mailbox into a newly created GL
// texture, after synchronization with the sync token. Returns the GL texture.
// |mailbox| is set to the imported mailbox.
GLuint ImportVideoFrameSingleMailbox(gpu::gles2::GLES2Interface* gl,
                                     VideoFrame* video_frame,
                                     gpu::Mailbox* mailbox) {
  const gpu::MailboxHolder& mailbox_holder =
      GetVideoFrameMailboxHolder(video_frame);
  *mailbox = mailbox_holder.mailbox;
  return SynchronizeAndImportMailbox(gl, mailbox_holder.sync_token, *mailbox);
}

gpu::Mailbox SynchronizeVideoFrameSingleMailbox(
    gpu::raster::RasterInterface* ri,
    VideoFrame* video_frame) {
  const gpu::MailboxHolder& mailbox_holder =
      GetVideoFrameMailboxHolder(video_frame);
  ri->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  return mailbox_holder.mailbox;
}

// Wraps a GL RGBA texture into a SkImage.
sk_sp<SkImage> WrapGLTexture(
    GLenum target,
    GLuint texture_id,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    viz::RasterContextProvider* raster_context_provider) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = target;
  // TODO(bsalomon): GrGLTextureInfo::fFormat and SkColorType passed to
  // SkImage factory should reflect video_frame->format(). Update once
  // Skia supports GL_RGB. skbug.com/7533
  texture_info.fFormat = GL_RGBA8_OES;
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   GrMipMapped::kNo, texture_info);
  return SkImage::MakeFromTexture(
      raster_context_provider->GrContext(), backend_texture,
      kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      color_space.ToSkColorSpace(), nullptr, nullptr);
}

void VideoFrameCopyTextureOrSubTexture(gpu::gles2::GLES2Interface* gl,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       GLuint source_texture,
                                       unsigned int target,
                                       unsigned int texture,
                                       unsigned int internal_format,
                                       unsigned int format,
                                       unsigned int type,
                                       int level,
                                       bool premultiply_alpha,
                                       bool flip_y) {
  // The video is stored in a unmultiplied format, so premultiply if necessary.
  // Application itself needs to take care of setting the right |flip_y|
  // value down to get the expected result.
  // "flip_y == true" means to reverse the video orientation while
  // "flip_y == false" means to keep the intrinsic orientation.
  if (visible_rect != gfx::Rect(coded_size)) {
    // Must reallocate the destination texture and copy only a sub-portion.

    // There should always be enough data in the source texture to
    // cover this copy.
    DCHECK_LE(visible_rect.width(), coded_size.width());
    DCHECK_LE(visible_rect.height(), coded_size.height());

    gl->BindTexture(target, texture);
    gl->TexImage2D(target, level, internal_format, visible_rect.width(),
                   visible_rect.height(), 0, format, type, nullptr);
    gl->CopySubTextureCHROMIUM(source_texture, 0, target, texture, level, 0, 0,
                               visible_rect.x(), visible_rect.y(),
                               visible_rect.width(), visible_rect.height(),
                               flip_y, premultiply_alpha, false);

  } else {
    gl->CopyTextureCHROMIUM(source_texture, 0, target, texture, level,
                            internal_format, type, flip_y, premultiply_alpha,
                            false);
  }
}

void OnQueryDone(scoped_refptr<VideoFrame> video_frame,
                 gpu::raster::RasterInterface* ri,
                 unsigned query_id) {
  ri->DeleteQueriesEXT(1, &query_id);
  // |video_frame| is dropped here.
}

void SynchronizeVideoFrameRead(scoped_refptr<VideoFrame> video_frame,
                               gpu::raster::RasterInterface* ri,
                               gpu::ContextSupport* context_support) {
  DCHECK(ri);
  SyncTokenClientImpl client(ri);
  video_frame->UpdateReleaseSyncToken(&client);

  if (video_frame->metadata()->IsTrue(
          VideoFrameMetadata::READ_LOCK_FENCES_ENABLED)) {
    // |video_frame| must be kept alive during read operations.
    DCHECK(context_support);
    unsigned query_id = 0;
    ri->GenQueriesEXT(1, &query_id);
    DCHECK(query_id);
    ri->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id);
    ri->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
    context_support->SignalQuery(
        query_id, base::BindOnce(&OnQueryDone, video_frame, ri, query_id));
  }
}

// TODO(thomasanderson): Remove these and use std::gcd and std::lcm once we're
// building with C++17.
size_t GCD(size_t a, size_t b) {
  return a == 0 ? b : GCD(b % a, a);
}
size_t LCM(size_t a, size_t b) {
  return a / GCD(a, b) * b;
}

void ConvertVideoFrameToRGBPixelsTask(const VideoFrame* video_frame,
                                      void* rgb_pixels,
                                      size_t row_bytes,
                                      size_t task_index,
                                      size_t n_tasks,
                                      base::RepeatingClosure* done) {
  const VideoPixelFormat format = video_frame->format();
  const int width = video_frame->visible_rect().width();
  const int height = video_frame->visible_rect().height();

  size_t rows_per_chunk = 1;
  for (size_t plane = 0; plane < VideoFrame::kMaxPlanes; ++plane) {
    if (VideoFrame::IsValidPlane(format, plane)) {
      rows_per_chunk =
          LCM(rows_per_chunk, VideoFrame::SampleSize(format, plane).height());
    }
  }

  base::CheckedNumeric<size_t> chunks = height / rows_per_chunk;
  const size_t chunk_start = (chunks * task_index / n_tasks).ValueOrDie();
  const size_t chunk_end = (chunks * (task_index + 1) / n_tasks).ValueOrDie();

  // Indivisible heights must process any remaining rows in the last task.
  size_t rows = (chunk_end - chunk_start) * rows_per_chunk;
  if (task_index + 1 == n_tasks)
    rows += height % rows_per_chunk;

  struct {
    int stride;
    const uint8_t* data;
  } plane_meta[VideoFrame::kMaxPlanes];

  for (size_t plane = 0; plane < VideoFrame::kMaxPlanes; ++plane) {
    if (VideoFrame::IsValidPlane(format, plane)) {
      plane_meta[plane] = {
          // Note: Unlike |data|, stride does not need to be adjusted by the
          // visible rect and sample size. Adding the full frame stride to a
          // pixel on row N and column M will wrap to column M on row N + 1.
          .stride = video_frame->stride(plane),

          .data = video_frame->visible_data(plane) +
                  video_frame->stride(plane) * (chunk_start * rows_per_chunk) /
                      VideoFrame::SampleSize(format, plane).height()};
    }
  }

  uint8_t* pixels = static_cast<uint8_t*>(rgb_pixels) +
                    row_bytes * chunk_start * rows_per_chunk;

  // TODO(hubbe): This should really default to the rec709 colorspace.
  // https://crbug.com/828599
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(&color_space);

  if (!video_frame->data(VideoFrame::kUPlane) &&
      !video_frame->data(VideoFrame::kVPlane)) {
    DCHECK_EQ(video_frame->format(), PIXEL_FORMAT_I420);
    auto func = (color_space == kJPEG_SkYUVColorSpace) ? LIBYUV_J400_TO_ARGB
                                                       : LIBYUV_I400_TO_ARGB;
    func(plane_meta[VideoFrame::kYPlane].data,
         plane_meta[VideoFrame::kYPlane].stride, pixels, row_bytes, width,
         rows);
    done->Run();
    return;
  }

  auto convert_yuv = [&](auto&& func) {
    func(plane_meta[VideoFrame::kYPlane].data,
         plane_meta[VideoFrame::kYPlane].stride,
         plane_meta[VideoFrame::kUPlane].data,
         plane_meta[VideoFrame::kUPlane].stride,
         plane_meta[VideoFrame::kVPlane].data,
         plane_meta[VideoFrame::kVPlane].stride, pixels, row_bytes, width,
         rows);
  };

  auto convert_yuv16 = [&](auto&& func) {
    func(
        reinterpret_cast<const uint16_t*>(plane_meta[VideoFrame::kYPlane].data),
        plane_meta[VideoFrame::kYPlane].stride / 2,
        reinterpret_cast<const uint16_t*>(plane_meta[VideoFrame::kUPlane].data),
        plane_meta[VideoFrame::kUPlane].stride / 2,
        reinterpret_cast<const uint16_t*>(plane_meta[VideoFrame::kVPlane].data),
        plane_meta[VideoFrame::kVPlane].stride / 2, pixels, row_bytes, width,
        rows);
  };

  switch (video_frame->format()) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      switch (color_space) {
        case kJPEG_SkYUVColorSpace:
          convert_yuv(LIBYUV_J420_TO_ARGB);
          break;
        case kRec709_SkYUVColorSpace:
          convert_yuv(LIBYUV_H420_TO_ARGB);
          break;
        case kRec601_SkYUVColorSpace:
          convert_yuv(LIBYUV_I420_TO_ARGB);
          break;
        case kBT2020_SkYUVColorSpace:
          convert_yuv(LIBYUV_U420_TO_ARGB);
          break;
        default:
          NOTREACHED();
      }
      break;
    case PIXEL_FORMAT_I422:
      switch (color_space) {
        case kJPEG_SkYUVColorSpace:
          convert_yuv(LIBYUV_J422_TO_ARGB);
          break;
        case kRec709_SkYUVColorSpace:
          convert_yuv(LIBYUV_H422_TO_ARGB);
          break;
        case kRec601_SkYUVColorSpace:
          convert_yuv(LIBYUV_I422_TO_ARGB);
          break;
        case kBT2020_SkYUVColorSpace:
          convert_yuv(LIBYUV_U422_TO_ARGB);
          break;
        default:
          NOTREACHED();
      }
      break;

    case PIXEL_FORMAT_I420A:
      LIBYUV_I420ALPHA_TO_ARGB(
          plane_meta[VideoFrame::kYPlane].data,
          plane_meta[VideoFrame::kYPlane].stride,
          plane_meta[VideoFrame::kUPlane].data,
          plane_meta[VideoFrame::kUPlane].stride,
          plane_meta[VideoFrame::kVPlane].data,
          plane_meta[VideoFrame::kVPlane].stride,
          plane_meta[VideoFrame::kAPlane].data,
          plane_meta[VideoFrame::kAPlane].stride, pixels, row_bytes, width,
          rows, 1);  // 1 = enable RGB premultiplication by Alpha.
      break;

    case PIXEL_FORMAT_I444:
      switch (color_space) {
        case kJPEG_SkYUVColorSpace:
          convert_yuv(LIBYUV_J444_TO_ARGB);
          break;
        case kRec709_SkYUVColorSpace:
          convert_yuv(LIBYUV_H444_TO_ARGB);
          break;
        case kRec601_SkYUVColorSpace:
          convert_yuv(LIBYUV_I444_TO_ARGB);
          break;
        case kBT2020_SkYUVColorSpace:
          convert_yuv(LIBYUV_U444_TO_ARGB);
          break;
        default:
          NOTREACHED();
      }
      break;

    case PIXEL_FORMAT_YUV420P10:
      switch (color_space) {
        case kRec709_SkYUVColorSpace:
          convert_yuv16(LIBYUV_H010_TO_ARGB);
          break;
        case kJPEG_SkYUVColorSpace:
          FALLTHROUGH;
        case kRec601_SkYUVColorSpace:
          convert_yuv16(LIBYUV_I010_TO_ARGB);
          break;
        case kBT2020_SkYUVColorSpace:
          convert_yuv16(LIBYUV_U010_TO_ARGB);
          break;
        default:
          NOTREACHED();
      }
      break;
    case PIXEL_FORMAT_YUV422P10:
      switch (color_space) {
        case kRec709_SkYUVColorSpace:
          convert_yuv16(LIBYUV_H210_TO_ARGB);
          break;
        case kJPEG_SkYUVColorSpace:
          FALLTHROUGH;
        case kRec601_SkYUVColorSpace:
          convert_yuv16(LIBYUV_I210_TO_ARGB);
          break;
        case kBT2020_SkYUVColorSpace:
          convert_yuv16(LIBYUV_U210_TO_ARGB);
          break;
        default:
          NOTREACHED();
      }
      break;
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
      NOTREACHED() << "These cases should be handled above";
      break;

    case PIXEL_FORMAT_NV12:
      LIBYUV_NV12_TO_ARGB(plane_meta[VideoFrame::kYPlane].data,
                          plane_meta[VideoFrame::kYPlane].stride,
                          plane_meta[VideoFrame::kUPlane].data,
                          plane_meta[VideoFrame::kUPlane].stride, pixels,
                          row_bytes, width, rows);
      break;

    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_P016LE:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED() << "Only YUV formats and Y16 are supported, got: "
                   << media::VideoPixelFormatToString(video_frame->format());
  }
  done->Run();
}

}  // anonymous namespace

// Generates an RGB image from a VideoFrame. Convert YUV to RGB plain on GPU.
class VideoImageGenerator : public cc::PaintImageGenerator {
 public:
  VideoImageGenerator(scoped_refptr<VideoFrame> frame)
      : cc::PaintImageGenerator(
            SkImageInfo::MakeN32Premul(frame->visible_rect().width(),
                                       frame->visible_rect().height())),
        frame_(std::move(frame)) {
    DCHECK(!frame_->HasTextures());
  }
  ~VideoImageGenerator() override = default;

  sk_sp<SkData> GetEncodedData() const override { return nullptr; }

  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t row_bytes,
                 size_t frame_index,
                 cc::PaintImage::GeneratorClientId client_id,
                 uint32_t lazy_pixel_ref) override {
    DCHECK_EQ(frame_index, 0u);

    // If skia couldn't do the YUV conversion on GPU, we will on CPU.
    PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(frame_.get(), pixels,
                                                           row_bytes);
    return true;
  }

  bool QueryYUVA8(SkYUVASizeInfo* sizeInfo,
                  SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
                  SkYUVColorSpace* color_space) const override {
    // Temporarily disabling this path to avoid creating YUV ImageData in
    // GpuImageDecodeCache.
    // TODO(crbug.com/921636): Restore the code below once YUV rendering support
    // is added for VideoImageGenerator.
    return false;
#if 0
    if (!media::IsYuvPlanar(frame_->format()) ||
        // TODO(rileya): Skia currently doesn't support YUVA conversion. Remove
        // this case once it does. As-is we will fall back on the pure-software
        // path in this case.
        frame_->format() == PIXEL_FORMAT_I420A) {
      return false;
    }

    if (color_space) {
      if (!frame_->ColorSpace().ToSkYUVColorSpace(color_space)) {
        // TODO(hubbe): This really should default to rec709
        // https://crbug.com/828599
        *color_space = kRec601_SkYUVColorSpace;
      }
    }

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      const gfx::Size size =
          VideoFrame::PlaneSize(frame_->format(), plane,
                                gfx::Size(frame_->visible_rect().width(),
                                          frame_->visible_rect().height()));
      sizeInfo->fSizes[plane].set(size.width(), size.height());
      sizeInfo->fWidthBytes[plane] = size.width();
    }
    sizeInfo->fSizes[VideoFrame::kAPlane] = SkISize::MakeEmpty();
    sizeInfo->fWidthBytes[VideoFrame::kAPlane] = 0;

    indices[SkYUVAIndex::kY_Index] = {VideoFrame::kYPlane, SkColorChannel::kR};
    indices[SkYUVAIndex::kU_Index] = {VideoFrame::kUPlane, SkColorChannel::kR};
    indices[SkYUVAIndex::kV_Index] = {VideoFrame::kVPlane, SkColorChannel::kR};
    indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kR};

    return true;
#endif
  }

  bool GetYUVA8Planes(const SkYUVASizeInfo& sizeInfo,
                      const SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
                      void* planes[4],
                      size_t frame_index,
                      uint32_t lazy_pixel_ref) override {
    DCHECK_EQ(frame_index, 0u);

    media::VideoPixelFormat format = frame_->format();
    DCHECK(media::IsYuvPlanar(format) && format != PIXEL_FORMAT_I420A);

    for (int i = 0; i <= VideoFrame::kVPlane; ++i) {
      if (sizeInfo.fSizes[i].isEmpty() || !sizeInfo.fWidthBytes[i]) {
        return false;
      }
    }
    if (!sizeInfo.fSizes[VideoFrame::kAPlane].isEmpty() ||
        sizeInfo.fWidthBytes[VideoFrame::kAPlane]) {
      return false;
    }
    int numPlanes;
    if (!SkYUVAIndex::AreValidIndices(indices, &numPlanes) || numPlanes != 3) {
      return false;
    }

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      const gfx::Size size =
          VideoFrame::PlaneSize(frame_->format(), plane,
                                gfx::Size(frame_->visible_rect().width(),
                                          frame_->visible_rect().height()));
      if (size.width() != sizeInfo.fSizes[plane].width() ||
          size.height() != sizeInfo.fSizes[plane].height()) {
        return false;
      }

      size_t offset;
      const int y_shift =
          (frame_->format() == media::PIXEL_FORMAT_I422) ? 0 : 1;
      if (plane == VideoFrame::kYPlane) {
        offset =
            (frame_->stride(VideoFrame::kYPlane) * frame_->visible_rect().y()) +
            frame_->visible_rect().x();
      } else {
        offset = (frame_->stride(VideoFrame::kUPlane) *
                  (frame_->visible_rect().y() >> y_shift)) +
                 (frame_->visible_rect().x() >> 1);
      }

      // Copy the frame to the supplied memory.
      // TODO: Find a way (API change?) to avoid this copy.
      char* out_line = static_cast<char*>(planes[plane]);
      int out_line_stride = sizeInfo.fWidthBytes[plane];
      uint8_t* in_line = frame_->data(plane) + offset;
      int in_line_stride = frame_->stride(plane);
      int plane_height = sizeInfo.fSizes[plane].height();
      if (in_line_stride == out_line_stride) {
        memcpy(out_line, in_line, plane_height * in_line_stride);
      } else {
        // Different line padding so need to copy one line at a time.
        int bytes_to_copy_per_line =
            out_line_stride < in_line_stride ? out_line_stride : in_line_stride;
        for (int line_no = 0; line_no < plane_height; line_no++) {
          memcpy(out_line, in_line, bytes_to_copy_per_line);
          in_line += in_line_stride;
          out_line += out_line_stride;
        }
      }
    }
    return true;
  }

 private:
  scoped_refptr<VideoFrame> frame_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoImageGenerator);
};

class VideoTextureBacking : public cc::TextureBacking {
 public:
  explicit VideoTextureBacking(sk_sp<SkImage> sk_image)
      : sk_image_(std::move(sk_image)) {}

  const SkImageInfo& GetSkImageInfo() override {
    return sk_image_->imageInfo();
  }
  gpu::Mailbox GetMailbox() const override { return mailbox_; }
  sk_sp<SkImage> GetAcceleratedSkImage() override { return sk_image_; }

 private:
  const sk_sp<SkImage> sk_image_;
  const gpu::Mailbox mailbox_;
};

PaintCanvasVideoRenderer::PaintCanvasVideoRenderer()
    : cache_deleting_timer_(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kTemporaryResourceDeletionDelay),
          this,
          &PaintCanvasVideoRenderer::ResetCache),
      renderer_stable_id_(cc::PaintImage::GetNextId()) {}

PaintCanvasVideoRenderer::~PaintCanvasVideoRenderer() = default;

void PaintCanvasVideoRenderer::Paint(
    scoped_refptr<VideoFrame> video_frame,
    cc::PaintCanvas* canvas,
    const gfx::RectF& dest_rect,
    cc::PaintFlags& flags,
    VideoTransformation video_transformation,
    viz::RasterContextProvider* raster_context_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (flags.getAlpha() == 0) {
    return;
  }

  SkRect dest;
  dest.setLTRB(dest_rect.x(), dest_rect.y(), dest_rect.right(),
               dest_rect.bottom());

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame.get() || video_frame->natural_size().IsEmpty() ||
      !(media::IsYuvPlanar(video_frame->format()) ||
        video_frame->format() == media::PIXEL_FORMAT_Y16 ||
        video_frame->HasTextures())) {
    cc::PaintFlags black_with_alpha_flags;
    black_with_alpha_flags.setAlpha(flags.getAlpha());
    canvas->drawRect(dest, black_with_alpha_flags);
    canvas->flush();
    return;
  }

  // Don't allow wrapping the VideoFrame texture, as we want to be able to cache
  // the PaintImage, to avoid redundant readbacks if the canvas is software.
  if (!UpdateLastImage(video_frame, raster_context_provider,
                       false /* allow_wrap_texture */))
    return;
  DCHECK(cache_);
  cc::PaintImage image = cache_->paint_image;
  DCHECK(image);

  base::Optional<ScopedSharedImageAccess> source_access;
  if (video_frame->HasTextures()) {
    DCHECK(!cache_->source_mailbox.IsZero());
    DCHECK(cache_->source_texture);
    source_access.emplace(raster_context_provider->RasterInterface(),
                          cache_->source_texture, cache_->source_mailbox);
  }

  cc::PaintFlags video_flags;
  video_flags.setAlpha(flags.getAlpha());
  video_flags.setBlendMode(flags.getBlendMode());
  video_flags.setFilterQuality(flags.getFilterQuality());

  const bool need_rotation = video_transformation.rotation != VIDEO_ROTATION_0;
  const bool need_scaling =
      dest_rect.size() != gfx::SizeF(image.width(), image.height());
  const bool need_translation = !dest_rect.origin().IsOrigin();
  // TODO(tmathmeyer): apply horizontal / vertical mirroring if needed.
  bool need_transform = need_rotation || need_scaling || need_translation;
  if (need_transform) {
    canvas->save();
    canvas->translate(
        SkFloatToScalar(dest_rect.x() + (dest_rect.width() * 0.5f)),
        SkFloatToScalar(dest_rect.y() + (dest_rect.height() * 0.5f)));
    SkScalar angle = SkFloatToScalar(0.0f);
    switch (video_transformation.rotation) {
      case VIDEO_ROTATION_0:
        break;
      case VIDEO_ROTATION_90:
        angle = SkFloatToScalar(90.0f);
        break;
      case VIDEO_ROTATION_180:
        angle = SkFloatToScalar(180.0f);
        break;
      case VIDEO_ROTATION_270:
        angle = SkFloatToScalar(270.0f);
        break;
    }
    canvas->rotate(angle);

    gfx::SizeF rotated_dest_size = dest_rect.size();
    if (video_transformation.rotation == VIDEO_ROTATION_90 ||
        video_transformation.rotation == VIDEO_ROTATION_270) {
      rotated_dest_size =
          gfx::SizeF(rotated_dest_size.height(), rotated_dest_size.width());
    }
    canvas->scale(SkFloatToScalar(rotated_dest_size.width() / image.width()),
                  SkFloatToScalar(rotated_dest_size.height() / image.height()));
    canvas->translate(-SkFloatToScalar(image.width() * 0.5f),
                      -SkFloatToScalar(image.height() * 0.5f));
  }

  SkImageInfo info;
  size_t row_bytes;
  SkIPoint origin;
  void* pixels = nullptr;
  // This if is a special handling of video for SkiaPaintcanvas backend, where
  // the video does not need any transform and it is enough to draw the frame
  // directly into the skia canvas
  if (!need_transform && video_frame->IsMappable() &&
      flags.getAlpha() == SK_AlphaOPAQUE &&
      flags.getBlendMode() == SkBlendMode::kSrc &&
      flags.getFilterQuality() == kLow_SkFilterQuality &&
      (pixels = canvas->accessTopLayerPixels(&info, &row_bytes, &origin)) &&
      info.colorType() == kBGRA_8888_SkColorType) {
    const size_t offset = info.computeOffset(origin.x(), origin.y(), row_bytes);
    void* const pixels_offset = reinterpret_cast<char*>(pixels) + offset;
    ConvertVideoFrameToRGBPixels(video_frame.get(), pixels_offset, row_bytes);
  } else {
    canvas->drawImage(image, 0, 0, &video_flags);
  }

  if (need_transform)
    canvas->restore();
  // Make sure to flush so we can remove the videoframe from the generator.
  canvas->flush();

  if (video_frame->HasTextures()) {
    source_access.reset();
    // Synchronize |video_frame| with the read operations in UpdateLastImage(),
    // which are triggered by canvas->flush().
    SynchronizeVideoFrameRead(std::move(video_frame),
                              raster_context_provider->RasterInterface(),
                              raster_context_provider->ContextSupport());
  }
  // Because we are not retaining a reference to the VideoFrame, it would be
  // invalid for the cache to directly wrap its texture(s), as they will be
  // recycled.
  DCHECK(!cache_ || !cache_->wraps_video_frame_texture);
}

void PaintCanvasVideoRenderer::Copy(
    scoped_refptr<VideoFrame> video_frame,
    cc::PaintCanvas* canvas,
    viz::RasterContextProvider* raster_context_provider) {
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setFilterQuality(kLow_SkFilterQuality);

  auto dest_rect = gfx::RectF(gfx::SizeF(video_frame->visible_rect().size()));
  Paint(std::move(video_frame), canvas, dest_rect, flags,
        media::kNoTransformation, raster_context_provider);
}

namespace {

// libyuv doesn't support all 9-, 10- nor 12-bit pixel formats yet. This
// function creates a regular 8-bit video frame which we can give to libyuv.
scoped_refptr<VideoFrame> DownShiftHighbitVideoFrame(
    const VideoFrame* video_frame) {
  VideoPixelFormat format;
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV420P9:
      format = PIXEL_FORMAT_I420;
      break;

    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV422P9:
      format = PIXEL_FORMAT_I422;
      break;

    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV444P9:
      format = PIXEL_FORMAT_I444;
      break;

    default:
      NOTREACHED();
      return nullptr;
  }
  const int shift = video_frame->BitDepth() - 8;
  scoped_refptr<VideoFrame> ret = VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());

  ret->set_color_space(video_frame->ColorSpace());
  // Copy all metadata.
  // (May be enough to copy color space)
  ret->metadata()->MergeMetadataFrom(video_frame->metadata());

  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
    int width = ret->row_bytes(plane);
    const uint16_t* src =
        reinterpret_cast<const uint16_t*>(video_frame->data(plane));
    uint8_t* dst = ret->data(plane);
    if (!src) {
      // An AV1 monochrome (grayscale) frame has no U and V planes. Set all U
      // and V samples to the neutral value (128).
      DCHECK_NE(plane, VideoFrame::kYPlane);
      memset(dst, 128, ret->rows(plane) * ret->stride(plane));
      continue;
    }
    for (int row = 0; row < video_frame->rows(plane); row++) {
      for (int x = 0; x < width; x++) {
        dst[x] = src[x] >> shift;
      }
      src += video_frame->stride(plane) / 2;
      dst += ret->stride(plane);
    }
  }
  return ret;
}

// Converts 16-bit data to |out| buffer of specified GL |type|.
// When the |format| is RGBA, the converted value is fed as luminance.
void FlipAndConvertY16(const VideoFrame* video_frame,
                       uint8_t* out,
                       unsigned format,
                       unsigned type,
                       bool flip_y,
                       size_t output_row_bytes) {
  const uint8_t* row_head = video_frame->visible_data(0);
  const size_t stride = video_frame->stride(0);
  const int height = video_frame->visible_rect().height();
  for (int i = 0; i < height; ++i, row_head += stride) {
    uint8_t* out_row_head = flip_y ? out + output_row_bytes * (height - i - 1)
                                   : out + output_row_bytes * i;
    const uint16_t* row = reinterpret_cast<const uint16_t*>(row_head);
    const uint16_t* row_end = row + video_frame->visible_rect().width();
    if (type == GL_FLOAT) {
      float* out_row = reinterpret_cast<float*>(out_row_head);
      if (format == GL_RGBA) {
        while (row < row_end) {
          float gray_value = *row++ / 65535.f;
          *out_row++ = gray_value;
          *out_row++ = gray_value;
          *out_row++ = gray_value;
          *out_row++ = 1.0f;
        }
        continue;
      } else if (format == GL_RED) {
        while (row < row_end)
          *out_row++ = *row++ / 65535.f;
        continue;
      }
      // For other formats, hit NOTREACHED below.
    } else if (type == GL_UNSIGNED_BYTE) {
      // We take the upper 8 bits of 16-bit data and convert it as luminance to
      // ARGB.  We loose the precision here, but it is important not to render
      // Y16 as RG_88.  To get the full precision use float textures with WebGL1
      // and e.g. R16UI or R32F textures with WebGL2.
      DCHECK_EQ(static_cast<unsigned>(GL_RGBA), format);
      uint32_t* rgba = reinterpret_cast<uint32_t*>(out_row_head);
      while (row < row_end) {
        uint32_t gray_value = *row++ >> 8;
        *rgba++ = SkColorSetRGB(gray_value, gray_value, gray_value);
      }
      continue;
    }
    NOTREACHED() << "Unsupported Y16 conversion for format: 0x" << std::hex
                 << format << " and type: 0x" << std::hex << type;
  }
}

// Common functionality of PaintCanvasVideoRenderer's TexImage2D and
// TexSubImage2D. Allocates a buffer required for conversion and converts
// |frame| content to desired |format|. Returns true if calling glTex(Sub)Image
// is supported for provided |frame| format and parameters.
bool TexImageHelper(VideoFrame* frame,
                    unsigned format,
                    unsigned type,
                    bool flip_y,
                    scoped_refptr<DataBuffer>* temp_buffer) {
  unsigned output_bytes_per_pixel = 0;
  switch (frame->format()) {
    case PIXEL_FORMAT_Y16:
      // Converting single component unsigned short here to FLOAT luminance.
      switch (format) {
        case GL_RGBA:
          if (type == GL_FLOAT) {
            output_bytes_per_pixel = 4 * sizeof(GLfloat);
            break;
          }
          return false;
        case GL_RED:
          if (type == GL_FLOAT) {
            output_bytes_per_pixel = sizeof(GLfloat);
            break;
          }
          return false;
        default:
          return false;
      }
      break;
    default:
      return false;
  }

  size_t output_row_bytes =
      frame->visible_rect().width() * output_bytes_per_pixel;
  *temp_buffer =
      new DataBuffer(output_row_bytes * frame->visible_rect().height());
  FlipAndConvertY16(frame, (*temp_buffer)->writable_data(), format, type,
                    flip_y, output_row_bytes);
  return true;
}

// Upload the |frame| data to temporary texture of |temp_format|,
// |temp_internalformat| and |temp_type| and then copy intermediate texture
// subimage to destination |texture|. The destination |texture| is bound to the
// |target| before the call.
void TextureSubImageUsingIntermediate(unsigned target,
                                      unsigned texture,
                                      gpu::gles2::GLES2Interface* gl,
                                      VideoFrame* frame,
                                      int temp_internalformat,
                                      unsigned temp_format,
                                      unsigned temp_type,
                                      int level,
                                      int xoffset,
                                      int yoffset,
                                      bool flip_y,
                                      bool premultiply_alpha) {
  unsigned temp_texture = 0;
  gl->GenTextures(1, &temp_texture);
  gl->BindTexture(target, temp_texture);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->TexImage2D(target, 0, temp_internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, temp_format, temp_type,
                 frame->visible_data(0));
  gl->BindTexture(target, texture);
  gl->CopySubTextureCHROMIUM(temp_texture, 0, target, texture, level, 0, 0,
                             xoffset, yoffset, frame->visible_rect().width(),
                             frame->visible_rect().height(), flip_y,
                             premultiply_alpha, false);
  gl->DeleteTextures(1, &temp_texture);
}

}  // anonymous namespace

// static
void PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
    const VideoFrame* video_frame,
    void* rgb_pixels,
    size_t row_bytes) {
  if (!video_frame->IsMappable()) {
    NOTREACHED() << "Cannot extract pixels from non-CPU frame formats.";
    return;
  }

  scoped_refptr<VideoFrame> temporary_frame;
  // TODO(thomasanderson): Parallelize converting these formats.
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
      temporary_frame = DownShiftHighbitVideoFrame(video_frame);
      video_frame = temporary_frame.get();
      break;
    case PIXEL_FORMAT_YUV420P10:
      // In AV1, a monochrome (grayscale) frame is represented as a YUV 4:2:0
      // frame with no U and V planes. Since there are no 10-bit versions of
      // libyuv::I400ToARGB() and libyuv::J400ToARGB(), convert the frame to an
      // 8-bit YUV 4:2:0 frame with U and V planes.
      if (!video_frame->data(VideoFrame::kUPlane) &&
          !video_frame->data(VideoFrame::kVPlane)) {
        temporary_frame = DownShiftHighbitVideoFrame(video_frame);
        video_frame = temporary_frame.get();
      }
      break;
    case PIXEL_FORMAT_Y16:
      // Since it is grayscale conversion, we disregard
      // SK_PMCOLOR_BYTE_ORDER and always use GL_RGBA.
      FlipAndConvertY16(video_frame, static_cast<uint8_t*>(rgb_pixels), GL_RGBA,
                        GL_UNSIGNED_BYTE, false /*flip_y*/, row_bytes);
      return;
    default:
      break;
  }

  constexpr size_t kTaskBytes = 1024 * 1024;  // 1 MiB
  const size_t n_tasks = std::min<size_t>(
      std::max<size_t>(
          1, VideoFrame::AllocationSize(video_frame->format(),
                                        video_frame->visible_rect().size()) /
                 kTaskBytes),
      base::SysInfo::NumberOfProcessors());
  base::WaitableEvent event;
  base::RepeatingClosure barrier = base::BarrierClosure(
      n_tasks,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

  for (size_t i = 1; i < n_tasks; ++i) {
    base::PostTask(FROM_HERE,
                   base::BindOnce(ConvertVideoFrameToRGBPixelsTask,
                                  base::Unretained(video_frame), rgb_pixels,
                                  row_bytes, i, n_tasks, &barrier));
  }
  ConvertVideoFrameToRGBPixelsTask(video_frame, rgb_pixels, row_bytes, 0,
                                   n_tasks, &barrier);
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    event.Wait();
  }
}

// static
void PaintCanvasVideoRenderer::CopyVideoFrameSingleTextureToGLTexture(
    gpu::gles2::GLES2Interface* gl,
    VideoFrame* video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());

  gpu::Mailbox mailbox;
  uint32_t source_texture =
      ImportVideoFrameSingleMailbox(gl, video_frame, &mailbox);
  {
    ScopedSharedImageAccess access(gl, source_texture, mailbox);
    VideoFrameCopyTextureOrSubTexture(
        gl, video_frame->coded_size(), video_frame->visible_rect(),
        source_texture, target, texture, internal_format, format, type, level,
        premultiply_alpha, flip_y);
  }
  gl->DeleteTextures(1, &source_texture);
  gl->ShallowFlushCHROMIUM();
  // The caller must call SynchronizeVideoFrameRead() after this operation, but
  // we can't do that because we don't have the ContextSupport.
}

bool PaintCanvasVideoRenderer::CopyVideoFrameTexturesToGLTexture(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    scoped_refptr<VideoFrame> video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());
  if (video_frame->NumTextures() > 1 ||
      video_frame->metadata()->IsTrue(
          VideoFrameMetadata::READ_LOCK_FENCES_ENABLED)) {
    if (!raster_context_provider)
      return false;
    GrContext* gr_context = raster_context_provider->GrContext();
    if (!gr_context)
      return false;
    if (!UpdateLastImage(video_frame, raster_context_provider,
                         true /* allow_wrap_texture */)) {
      return false;
    }

    DCHECK(cache_);
    DCHECK(!cache_->source_mailbox.IsZero());
    gpu::raster::RasterInterface* canvas_ri =
        raster_context_provider->RasterInterface();

    gpu::SyncToken sync_token;
    // Wait for mailbox creation on canvas context before consuming it and
    // copying from it on the consumer context.
    canvas_ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

    uint32_t intermediate_texture = SynchronizeAndImportMailbox(
        destination_gl, sync_token, cache_->source_mailbox);
    {
      ScopedSharedImageAccess access(destination_gl, intermediate_texture,
                                     cache_->source_mailbox);
      VideoFrameCopyTextureOrSubTexture(
          destination_gl, cache_->coded_size, cache_->visible_rect,
          intermediate_texture, target, texture, internal_format, format, type,
          level, premultiply_alpha, flip_y);
    }

    destination_gl->DeleteTextures(1, &intermediate_texture);

    // Wait for destination context to consume mailbox before deleting it in
    // canvas context.
    gpu::SyncToken dest_sync_token;
    destination_gl->GenUnverifiedSyncTokenCHROMIUM(dest_sync_token.GetData());
    canvas_ri->WaitSyncTokenCHROMIUM(dest_sync_token.GetConstData());

    // Because we are not retaining a reference to the VideoFrame, it would be
    // invalid to keep the cache around if it directly wraps the VideoFrame
    // texture(s), as they will be recycled.
    if (cache_->wraps_video_frame_texture)
      cache_.reset();

    // Synchronize |video_frame| with the read operations in UpdateLastImage(),
    // which are triggered by getBackendTexture() or CopyTextureCHROMIUM (in the
    // case the cache was referencing its texture(s) directly).
    SynchronizeVideoFrameRead(std::move(video_frame), canvas_ri,
                              raster_context_provider->ContextSupport());
  } else {
    CopyVideoFrameSingleTextureToGLTexture(
        destination_gl, video_frame.get(), target, texture, internal_format,
        format, type, level, premultiply_alpha, flip_y);
    SyncTokenClientImpl client(destination_gl);
    video_frame->UpdateReleaseSyncToken(&client);
  }
  DCHECK(!cache_ || !cache_->wraps_video_frame_texture);

  return true;
}

bool PaintCanvasVideoRenderer::PrepareVideoFrameForWebGL(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    scoped_refptr<VideoFrame> video_frame,
    unsigned int target,
    unsigned int texture) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());
  if (video_frame->NumTextures() == 1) {
    if (target == GL_TEXTURE_EXTERNAL_OES) {
      // We don't support Android now.
      // TODO(crbug.com/776222): support Android.
      return false;
    }
    // We don't support sharing single video frame texture now.
    // TODO(crbug.com/776222): deal with single video frame texture.
    return false;
  }

  if (!raster_context_provider || !raster_context_provider->GrContext())
    return false;

  // Take webgl video texture as 2D texture. Setting it as external render
  // target backend for skia.
  destination_gl->BindTexture(target, texture);
  destination_gl->TexImage2D(target, 0, GL_RGBA,
                             video_frame->coded_size().width(),
                             video_frame->coded_size().height(), 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, nullptr);

  gpu::raster::RasterInterface* source_ri =
      raster_context_provider->RasterInterface();
  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.texture_target = target;
  destination_gl->ProduceTextureDirectCHROMIUM(texture,
                                               mailbox_holder.mailbox.name);

  destination_gl->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  if (!PrepareVideoFrame(video_frame, raster_context_provider,
                         mailbox_holder)) {
    return false;
  }

  // Wait for mailbox creation on canvas context before consuming it and
  // copying from it on the consumer context.
  source_ri->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  destination_gl->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());

  SyncTokenClientImpl client(source_ri);
  video_frame->UpdateReleaseSyncToken(&client);

  DCHECK(!cache_ || !cache_->wraps_video_frame_texture);
  return true;
}

bool PaintCanvasVideoRenderer::CopyVideoFrameYUVDataToGLTexture(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    const VideoFrame& video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(raster_context_provider);
  GrContext* gr_context = raster_context_provider->GrContext();
  if (!gr_context) {
    return false;
  }

  if (!video_frame.IsMappable()) {
    return false;
  }

  if (video_frame.format() != media::PIXEL_FORMAT_I420) {
    return false;
  }
  // Could handle NV12 here as well. See NewSkImageFromVideoFrameYUVTextures.

  static constexpr size_t kNumPlanes = 3;
  DCHECK_EQ(video_frame.NumPlanes(video_frame.format()), kNumPlanes);
  // Y,U,V GPU-side SkImages. (These must outlive the yuv_textures).
  sk_sp<SkImage> yuv_images[kNumPlanes]{};
  // Y,U,V GPU textures from those SkImages.
  // (A GrBackendTexture is a non-owned reference to the SkImage's texture.)
  GrBackendTexture yuv_textures[kNumPlanes]{};

  // Upload the whole coded image area (not visible rect).
  gfx::Size y_tex_size = video_frame.coded_size();
  gfx::Size uv_tex_size((y_tex_size.width() + 1) / 2,
                        (y_tex_size.height() + 1) / 2);

  for (size_t plane = 0; plane < kNumPlanes; ++plane) {
    const uint8_t* data = video_frame.data(plane);
    int plane_stride = video_frame.stride(plane);

    bool is_y_plane = plane == media::VideoFrame::kYPlane;
    gfx::Size tex_size = is_y_plane ? y_tex_size : uv_tex_size;
    int data_size = plane_stride * (tex_size.height() - 1) + tex_size.width();

    // Create a CPU-side SkImage from the channel.
    sk_sp<SkData> sk_data = SkData::MakeWithoutCopy(data, data_size);
    DCHECK(sk_data);
    SkImageInfo image_info =
        SkImageInfo::Make(tex_size.width(), tex_size.height(),
                          kGray_8_SkColorType, kUnknown_SkAlphaType);
    sk_sp<SkImage> plane_image_cpu =
        SkImage::MakeRasterData(image_info, sk_data, plane_stride);
    DCHECK(plane_image_cpu);

    // Upload the CPU-side SkImage into a GPU-side SkImage.
    // (Note the original video_frame data is no longer used after this point.)
    yuv_images[plane] = plane_image_cpu->makeTextureImage(gr_context);
    DCHECK(yuv_images[plane]);

    // Extract the backend texture from the GPU-side image.
    yuv_textures[plane] = yuv_images[plane]->getBackendTexture(false);
  }

  auto* sii = raster_context_provider->SharedImageInterface();
  gpu::raster::RasterInterface* source_ri =
      raster_context_provider->RasterInterface();

  // We need a shared image to receive the intermediate RGB result. Try to reuse
  // one if compatible, otherwise create a new one.
  if (yuv_cache_.texture && yuv_cache_.size == video_frame.coded_size() &&
      yuv_cache_.raster_context_provider == raster_context_provider) {
    source_ri->WaitSyncTokenCHROMIUM(yuv_cache_.sync_token.GetConstData());
  } else {
    yuv_cache_.Reset();
    yuv_cache_.raster_context_provider = raster_context_provider;
    yuv_cache_.size = video_frame.coded_size();
    yuv_cache_.mailbox = sii->CreateSharedImage(
        viz::ResourceFormat::RGBA_8888, video_frame.coded_size(),
        gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
    yuv_cache_.texture = SynchronizeAndImportMailbox(
        source_ri, sii->GenUnverifiedSyncToken(), yuv_cache_.mailbox);
  }

  // On the source GL context, do the YUV->RGB conversion using Skia.
  gpu::SyncToken post_conversion_sync_token;
  {
    source_ri->BeginSharedImageAccessDirectCHROMIUM(
        yuv_cache_.texture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

    GrGLTextureInfo backend_texture = {};
    backend_texture.fTarget = GL_TEXTURE_2D;
    backend_texture.fID = yuv_cache_.texture;
    backend_texture.fFormat = GL_RGBA8;
    GrBackendTexture result_texture(video_frame.coded_size().width(),
                                    video_frame.coded_size().height(),
                                    GrMipMapped::kNo, backend_texture);

    sk_sp<SkImage> yuv_image = YUVGrBackendTexturesToSkImage(
        gr_context, video_frame.ColorSpace(), video_frame.format(),
        yuv_textures, result_texture);

    gr_context->flush();
    source_ri->EndSharedImageAccessDirectCHROMIUM(yuv_cache_.texture);

    source_ri->GenUnverifiedSyncTokenCHROMIUM(
        post_conversion_sync_token.GetData());

    if (!yuv_image) {
      // Conversion failed. Note the last use sync token for destruction.
      yuv_cache_.sync_token = post_conversion_sync_token;
      yuv_cache_.Reset();
      return false;
    }
  }

  // On the destination GL context, do a copy (with cropping) into the
  // destination texture.
  GLuint intermediate_texture = SynchronizeAndImportMailbox(
      destination_gl, post_conversion_sync_token, yuv_cache_.mailbox);
  {
    ScopedSharedImageAccess access(destination_gl, intermediate_texture,
                                   yuv_cache_.mailbox);
    VideoFrameCopyTextureOrSubTexture(
        destination_gl, video_frame.coded_size(), video_frame.visible_rect(),
        intermediate_texture, target, texture, internal_format, format, type,
        level, premultiply_alpha, flip_y);
  }
  destination_gl->DeleteTextures(1, &intermediate_texture);
  destination_gl->GenUnverifiedSyncTokenCHROMIUM(
      yuv_cache_.sync_token.GetData());

  // video_frame->UpdateReleaseSyncToken is not necessary since the video frame
  // data we used was CPU-side (IsMappable) to begin with. If there were any
  // textures, we didn't use them.

  // The temporary SkImages should be automatically cleaned up here.

  // Kick off a timer to release the cache.
  cache_deleting_timer_.Reset();
  return true;
}

bool PaintCanvasVideoRenderer::TexImage2D(
    unsigned target,
    unsigned texture,
    gpu::gles2::GLES2Interface* gl,
    const gpu::Capabilities& gpu_capabilities,
    VideoFrame* frame,
    int level,
    int internalformat,
    unsigned format,
    unsigned type,
    bool flip_y,
    bool premultiply_alpha) {
  DCHECK(frame);
  DCHECK(!frame->HasTextures());

  // Note: CopyTextureCHROMIUM uses mediump for color computation. Don't use
  // it if the precision would lead to data loss when converting 16-bit
  // normalized to float. medium_float.precision > 15 means that the approach
  // below is not used on Android, where the extension EXT_texture_norm16 is
  // not widely supported. It is used on Windows, Linux and OSX.
  // Android support is not required for now because Tango depth camera already
  // provides floating point data (projected point cloud). See crbug.com/674440.
  if (gpu_capabilities.texture_norm16 &&
      gpu_capabilities.fragment_shader_precisions.medium_float.precision > 15 &&
      target == GL_TEXTURE_2D &&
      (type == GL_FLOAT || type == GL_UNSIGNED_BYTE)) {
    // TODO(aleksandar.stojiljkovic): Extend the approach to TexSubImage2D
    // implementation and other types. See https://crbug.com/624436.

    // Allocate the destination texture.
    gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                   frame->visible_rect().height(), 0, format, type, nullptr);
    // We use sized internal format GL_R16_EXT instead of unsized GL_RED.
    // See angleproject:1952
    TextureSubImageUsingIntermediate(target, texture, gl, frame, GL_R16_EXT,
                                     GL_RED, GL_UNSIGNED_SHORT, level, 0, 0,
                                     flip_y, premultiply_alpha);
    return true;
  }
  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, flip_y, &temp_buffer))
    return false;

  gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, format, type,
                 temp_buffer->data());
  return true;
}

bool PaintCanvasVideoRenderer::TexSubImage2D(unsigned target,
                                             gpu::gles2::GLES2Interface* gl,
                                             VideoFrame* frame,
                                             int level,
                                             unsigned format,
                                             unsigned type,
                                             int xoffset,
                                             int yoffset,
                                             bool flip_y,
                                             bool premultiply_alpha) {
  DCHECK(frame);
  DCHECK(!frame->HasTextures());

  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, flip_y, &temp_buffer))
    return false;

  gl->TexSubImage2D(
      target, level, xoffset, yoffset, frame->visible_rect().width(),
      frame->visible_rect().height(), format, type, temp_buffer->data());
  return true;
}

void PaintCanvasVideoRenderer::ResetCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  cache_.reset();
  yuv_cache_.Reset();
}

PaintCanvasVideoRenderer::Cache::Cache(int frame_id) : frame_id(frame_id) {}

PaintCanvasVideoRenderer::Cache::~Cache() {
  if (!raster_context_provider)
    return;

  DCHECK(!source_mailbox.IsZero());
  DCHECK(source_texture);
  auto* ri = raster_context_provider->RasterInterface();
  if (!texture_ownership_in_skia)
    ri->DeleteGpuRasterTexture(source_texture);
  if (!wraps_video_frame_texture) {
    gpu::SyncToken sync_token;
    ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    auto* sii = raster_context_provider->SharedImageInterface();
    sii->DestroySharedImage(sync_token, source_mailbox);
  }
}

bool PaintCanvasVideoRenderer::Cache::Recycle() {
  if (!texture_ownership_in_skia)
    return true;

  auto sk_image = paint_image.GetSkImage();
  paint_image = cc::PaintImage();
  if (!sk_image->unique())
    return false;

  // Flush any pending GPU work using this texture.
  sk_image->flush(raster_context_provider->GrContext());

  // We need a new texture ID because skia will destroy the previous one with
  // the SkImage.
  texture_ownership_in_skia = false;
  source_texture = 0;
  return true;
}

bool PaintCanvasVideoRenderer::UpdateLastImage(
    scoped_refptr<VideoFrame> video_frame,
    viz::RasterContextProvider* raster_context_provider,
    bool allow_wrap_texture) {
  DCHECK(!cache_ || !cache_->wraps_video_frame_texture);
  if (!cache_ || video_frame->unique_id() != cache_->frame_id ||
      cache_->source_mailbox.IsZero()) {
    auto paint_image_builder =
        cc::PaintImageBuilder::WithDefault()
            .set_id(renderer_stable_id_)
            .set_animation_type(cc::PaintImage::AnimationType::VIDEO)
            .set_completion_state(cc::PaintImage::CompletionState::DONE);

    // Generate a new image.
    // Note: Skia will hold onto |video_frame| via |video_generator| only when
    // |video_frame| is software.
    // Holding |video_frame| longer than this call when using GPUVideoDecoder
    // could cause problems since the pool of VideoFrames has a fixed size.
    if (video_frame->HasTextures()) {
      DCHECK(raster_context_provider);
      DCHECK(raster_context_provider->GrContext());
      auto* ri = raster_context_provider->RasterInterface();
      DCHECK(ri);

      if (allow_wrap_texture && video_frame->NumTextures() == 1) {
        cache_.emplace(video_frame->unique_id());
        const gpu::MailboxHolder& holder =
            GetVideoFrameMailboxHolder(video_frame.get());
        cache_->source_mailbox = holder.mailbox;
        ri->WaitSyncTokenCHROMIUM(holder.sync_token.GetConstData());
        cache_->wraps_video_frame_texture = true;
      } else {
        if (cache_ &&
            cache_->raster_context_provider == raster_context_provider &&
            cache_->coded_size == video_frame->coded_size() &&
            cache_->Recycle()) {
          // We can reuse the shared image from the previous cache.
          cache_->frame_id = video_frame->unique_id();
        } else {
          cache_.emplace(video_frame->unique_id());
          auto* sii = raster_context_provider->SharedImageInterface();

          // TODO(nazabris): Sort out what to do when GLES2 is needed but the
          // cached shared image is created without it.
          uint32_t flags =
              gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER;
          if (raster_context_provider->ContextCapabilities()
                  .supports_oop_raster) {
            flags |= gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
          }
          cache_->source_mailbox = sii->CreateSharedImage(
              viz::ResourceFormat::RGBA_8888, video_frame->coded_size(),
              gfx::ColorSpace(), flags);
          ri->WaitSyncTokenCHROMIUM(
              sii->GenUnverifiedSyncToken().GetConstData());
        }

        DCHECK(!cache_->texture_ownership_in_skia);
        if (video_frame->NumTextures() == 1) {
          auto frame_mailbox =
              SynchronizeVideoFrameSingleMailbox(ri, video_frame.get());
          ri->CopySubTexture(
              frame_mailbox, cache_->source_mailbox, GL_TEXTURE_2D, 0, 0, 0, 0,
              video_frame->coded_size().width(),
              video_frame->coded_size().height(), GL_FALSE, GL_FALSE);
        } else {
          gpu::MailboxHolder dest_holder{cache_->source_mailbox,
                                         gpu::SyncToken(), GL_TEXTURE_2D};
          ConvertFromVideoFrameYUV(video_frame.get(), raster_context_provider,
                                   dest_holder);
        }
        raster_context_provider->GrContext()->flush();
      }

      // TODO(jochin): Don't always generate SkImage here.
      DCHECK(cache_->source_texture == 0);
      cache_->source_texture =
          ri->CreateAndConsumeForGpuRaster(cache_->source_mailbox);

      // TODO(nazabris): Handle scoped access correctly. This follows the
      // current pattern but is most likely bugged. Access should last for the
      // lifetime of the SkImage.
      ScopedSharedImageAccess(ri, cache_->source_texture,
                              cache_->source_mailbox);
      auto source_image =
          WrapGLTexture(cache_->wraps_video_frame_texture
                            ? video_frame->mailbox_holder(0).texture_target
                            : GL_TEXTURE_2D,
                        cache_->source_texture, video_frame->coded_size(),
                        video_frame->ColorSpace(), raster_context_provider);
      if (!source_image) {
        // Couldn't create the SkImage.
        cache_.reset();
        return false;
      }
      cache_->raster_context_provider = raster_context_provider;
      cache_->coded_size = video_frame->coded_size();
      cache_->visible_rect = video_frame->visible_rect();
      sk_sp<SkImage> source_subset =
          source_image->makeSubset(gfx::RectToSkIRect(cache_->visible_rect));
      if (source_subset) {
        // We use the flushPendingGrContextIO = true so we can flush any pending
        // GPU work on the GrContext to ensure that skia exectues the work for
        // generating the subset and it can be safely destroyed.
        GrBackendTexture image_backend =
            source_image->getBackendTexture(/*flushPendingGrContextIO*/ true);
        GrBackendTexture subset_backend =
            source_subset->getBackendTexture(/*flushPendingGrContextIO*/ true);
#if DCHECK_IS_ON()
        GrGLTextureInfo backend_info;
        if (image_backend.getGLTextureInfo(&backend_info))
          DCHECK_EQ(backend_info.fID, cache_->source_texture);
#endif
        if (subset_backend.isValid() &&
            subset_backend.isSameTexture(image_backend)) {
          cache_->texture_ownership_in_skia = true;
          source_subset = SkImage::MakeFromAdoptedTexture(
              cache_->raster_context_provider->GrContext(), image_backend,
              kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
              kPremul_SkAlphaType, source_image->imageInfo().refColorSpace());
        }
      }
      paint_image_builder.set_texture_backing(
          sk_sp<VideoTextureBacking>(
              new VideoTextureBacking(std::move(source_subset))),
          cc::PaintImage::GetNextContentId());
    } else {
      cache_.emplace(video_frame->unique_id());
      paint_image_builder.set_paint_image_generator(
          sk_make_sp<VideoImageGenerator>(video_frame));
    }
    cache_->paint_image = paint_image_builder.TakePaintImage();
    if (!cache_->paint_image) {
      // Couldn't create the SkImage.
      cache_.reset();
      return false;
    }
  }

  DCHECK(cache_);
  cache_deleting_timer_.Reset();
  return true;
}

bool PaintCanvasVideoRenderer::PrepareVideoFrame(
    scoped_refptr<VideoFrame> video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_holder) {
  // Generate a new image.
  // Note: Skia will hold onto |video_frame| via |video_generator| only when
  // |video_frame| is software.
  // Holding |video_frame| longer than this call when using GPUVideoDecoder
  // could cause problems since the pool of VideoFrames has a fixed size.
  if (video_frame->HasTextures()) {
    if (video_frame->NumTextures() > 1) {
      ConvertFromVideoFrameYUV(video_frame.get(), raster_context_provider,
                               dest_holder);
    } else {
      // We don't support Android now.
      return false;
    }
  }
  return true;
}

PaintCanvasVideoRenderer::YUVTextureCache::YUVTextureCache() = default;
PaintCanvasVideoRenderer::YUVTextureCache::~YUVTextureCache() = default;

void PaintCanvasVideoRenderer::YUVTextureCache::Reset() {
  if (!texture)
    return;
  DCHECK(raster_context_provider);

  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  ri->DeleteGpuRasterTexture(texture);
  texture = 0;
  ri->OrderingBarrierCHROMIUM();

  auto* sii = raster_context_provider->SharedImageInterface();
  sii->DestroySharedImage(sync_token, mailbox);

  // Kick off the GL work up to the OrderingBarrierCHROMIUM above as well as the
  // SharedImageInterface work, to ensure the shared image memory is released in
  // a timely fashion.
  raster_context_provider->ContextSupport()->FlushPendingWork();
  raster_context_provider.reset();
}

gfx::Size PaintCanvasVideoRenderer::LastImageDimensionsForTesting() {
  DCHECK(cache_);
  DCHECK(cache_->paint_image);
  return gfx::Size(cache_->paint_image.width(), cache_->paint_image.height());
}

}  // namespace media
