// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_OPENGL_GL_CURSOR_FEEDBACK_H_
#define REMOTING_CLIENT_OPENGL_GL_CURSOR_FEEDBACK_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"

namespace remoting {

class GlCanvas;
class GlRenderLayer;

// This class draws the cursor feedback on the canvas.
class GlCursorFeedback {
 public:
  GlCursorFeedback();
  ~GlCursorFeedback();

  // Sets the canvas on which the cursor feedback will be drawn. Resumes the
  // feedback texture to the context of the new canvas.
  // If |canvas| is nullptr, nothing will happen when calling Draw().
  void SetCanvas(GlCanvas* canvas);

  void StartAnimation(float x, float y, float diameter);

  // Returns true if animation is not finished, false otherwise. Does nothing
  // if the animation has stopped.
  bool Draw();

 private:
  std::unique_ptr<GlRenderLayer> layer_;
  float max_diameter_ = 0;
  float cursor_x_ = 0;
  float cursor_y_ = 0;
  base::TimeTicks animation_start_time_;

  DISALLOW_COPY_AND_ASSIGN(GlCursorFeedback);
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_OPENGL_GL_CURSOR_FEEDBACK_H_
