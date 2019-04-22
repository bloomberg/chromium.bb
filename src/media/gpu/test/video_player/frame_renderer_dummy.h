// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_DUMMY_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_DUMMY_H_

#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/gpu/test/video_player/frame_renderer.h"

namespace media {
namespace test {

// The dummy frame renderer can be used when we're not interested in rendering
// the decoded frames to screen or file. No rate-limiting is done, and frames
// are consumed as fast as they are provided.
class FrameRendererDummy : public FrameRenderer {
 public:
  ~FrameRendererDummy() override;

  // Create an instance of the dummy frame renderer.
  static std::unique_ptr<FrameRendererDummy> Create();

  // FrameRenderer implementation
  void AcquireGLContext() override;
  void ReleaseGLContext() override;
  gl::GLContext* GetGLContext() override;
  void RenderFrame(scoped_refptr<VideoFrame> video_frame) override;
  scoped_refptr<VideoFrame> CreateVideoFrame(VideoPixelFormat pixel_format,
                                             const gfx::Size& size,
                                             uint32_t texture_target,
                                             uint32_t* texture_id) override;

 private:
  FrameRendererDummy();

  // Initialize the frame renderer, performs all rendering-related setup.
  bool Initialize();

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(FrameRendererDummy);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_DUMMY_H_
