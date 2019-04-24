// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encode_accelerator_unittest_helpers.h"

#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "media/gpu/test/texture_ref.h"
#include "media/gpu/test/video_frame_mapper_factory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/native_pixmap.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#endif

namespace media {
namespace test {

namespace {

#if defined(OS_CHROMEOS)
// Copy |src_frame| buffer to buffers referred by |dst_frame|.
bool BlitVideoFrame(scoped_refptr<VideoFrame> src_frame,
                    scoped_refptr<VideoFrame> dst_frame) {
  LOG_ASSERT(src_frame->storage_type() != VideoFrame::STORAGE_DMABUFS);

  scoped_refptr<VideoFrame> mapped_dst_frame = dst_frame;
  if (dst_frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    auto video_frame_mapper = test::VideoFrameMapperFactory::CreateMapper(true);
    LOG_ASSERT(video_frame_mapper);
    mapped_dst_frame = video_frame_mapper->Map(dst_frame);
    if (!mapped_dst_frame) {
      LOG(ERROR) << "Failed to map DMABuf video frame.";
      return false;
    }
  }

  LOG_ASSERT(src_frame->format() == dst_frame->format());
  size_t num_planes = VideoFrame::NumPlanes(src_frame->format());
  for (size_t i = 0; i < num_planes; i++) {
    size_t length = dst_frame->layout().planes()[i].stride *
                    VideoFrame::Rows(i, dst_frame->format(),
                                     dst_frame->coded_size().height());
    memcpy(mapped_dst_frame->data(i), src_frame->data(i), length);
  }
  return true;
}
#endif

}  // namespace

scoped_refptr<VideoFrame> CreateDmabufFrameFromVideoFrame(
    scoped_refptr<VideoFrame> frame) {
  scoped_refptr<VideoFrame> dmabuf_frame;
#if defined(OS_CHROMEOS)
  constexpr uint32_t kDummyTextureId = 0;
  auto texture_ref = test::TextureRef::CreatePreallocated(
      kDummyTextureId, base::DoNothing(), frame->format(), frame->coded_size(),
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  LOG_ASSERT(texture_ref);
  dmabuf_frame = texture_ref->ExportVideoFrame(frame->visible_rect());
  if (!dmabuf_frame) {
    LOG(ERROR) << "Failed to create video frame from texture_ref";
    return nullptr;
  }
  dmabuf_frame->set_timestamp(frame->timestamp());
  LOG_ASSERT(texture_ref->IsDirectlyMappable());
  if (!BlitVideoFrame(frame, dmabuf_frame)) {
    LOG(ERROR) << "Failed to copy mapped buffer to dmabuf fds";
    return nullptr;
  }
#endif
  return dmabuf_frame;
}

}  // namespace test
}  // namespace media
