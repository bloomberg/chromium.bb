// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_OPENGL_GL_DESKTOP_H_
#define REMOTING_CLIENT_OPENGL_GL_DESKTOP_H_

#include <memory>

#include "base/macros.h"

namespace webrtc {
class DesktopFrame;
class DesktopRegion;
}  // namespace webrtc

namespace remoting {

class GlCanvas;
class GlRenderLayer;

// This class draws the desktop on the canvas.
class GlDesktop {
 public:
  GlDesktop();
  virtual ~GlDesktop();

  void SetVideoFrame(std::unique_ptr<webrtc::DesktopFrame> frame);

  // Sets the canvas on which the desktop will be drawn. Resumes the current
  // state of the desktop to the context of the new canvas.
  // If |canvas| is nullptr, nothing will happen when calling Draw().
  void SetCanvas(GlCanvas* canvas);

  // Draws the desktop on the canvas.
  void Draw();

 private:
  std::unique_ptr<GlRenderLayer> layer_;
  std::unique_ptr<webrtc::DesktopFrame> last_frame_;

  DISALLOW_COPY_AND_ASSIGN(GlDesktop);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_OPENGL_GL_DESKTOP_H_
