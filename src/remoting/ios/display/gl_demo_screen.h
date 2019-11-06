// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DISPLAY_GL_DEMO_SCREEN_H_
#define REMOTING_IOS_DISPLAY_GL_DEMO_SCREEN_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/display/drawable.h"
#include "remoting/client/display/sys_opengl.h"

namespace remoting {

// This class draws the desktop on the canvas.
class GlDemoScreen : public Drawable {
 public:
  GlDemoScreen();
  ~GlDemoScreen() override;

  // Drawable implementation.
  void SetCanvas(base::WeakPtr<Canvas> canvas) override;
  bool Draw() override;
  base::WeakPtr<Drawable> GetWeakPtr() override;
  int GetZIndex() override;

 private:
  base::WeakPtr<Canvas> canvas_;
  int square_size_ = 0;
  GLuint program_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<Drawable> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlDemoScreen);
};

}  // namespace remoting

#endif  // REMOTING_IOS_DISPLAY_GL_DEMO_SCREEN_H_
