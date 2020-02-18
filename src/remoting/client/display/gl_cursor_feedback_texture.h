// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_CURSOR_FEEDBACK_TEXTURE_H_
#define REMOTING_CLIENT_DISPLAY_GL_CURSOR_FEEDBACK_TEXTURE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"

namespace remoting {

// This is a singleton for generating and holding the the cursor feedback
// texture.
class GlCursorFeedbackTexture {
 public:
  static const int kTextureWidth;

  static GlCursorFeedbackTexture* GetInstance();

  const std::vector<uint8_t>& GetTexture() const;

 private:
  GlCursorFeedbackTexture();
  ~GlCursorFeedbackTexture();

  friend struct base::DefaultSingletonTraits<GlCursorFeedbackTexture>;

  std::vector<uint8_t> texture_;

  DISALLOW_COPY_AND_ASSIGN(GlCursorFeedbackTexture);
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_DISPLAY_GL_CURSOR_FEEDBACK_TEXTURE_H_
