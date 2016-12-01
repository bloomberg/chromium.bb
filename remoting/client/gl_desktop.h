// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_OPENGL_GL_DESKTOP_H_
#define REMOTING_CLIENT_OPENGL_GL_DESKTOP_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class GlCanvas;

// This class draws the desktop on the canvas.
class GlDesktop {
 public:
  GlDesktop();
  virtual ~GlDesktop();

  // |frame| can be either a full frame or updated regions only frame.
  void SetVideoFrame(const webrtc::DesktopFrame& frame);

  // Sets the canvas on which the desktop will be drawn. Caller must feed a
  // full desktop frame after calling this function.
  // If |canvas| is nullptr, nothing will happen when calling Draw().
  void SetCanvas(GlCanvas* canvas);

  // Draws the desktop on the canvas.
  void Draw();

 private:
  struct GlDesktopTextureContainer;

  void ReallocateTextures(const webrtc::DesktopSize& size);

  std::vector<std::unique_ptr<GlDesktopTextureContainer>> textures_;

  webrtc::DesktopSize last_desktop_size_;
  int max_texture_size_ = 0;
  GlCanvas* canvas_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GlDesktop);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_OPENGL_GL_DESKTOP_H_
