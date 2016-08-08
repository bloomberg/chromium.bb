// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gl_cursor_feedback.h"

#include <math.h>
#include <array>

#include "base/logging.h"
#include "remoting/client/gl_canvas.h"
#include "remoting/client/gl_cursor_feedback_texture.h"
#include "remoting/client/gl_math.h"
#include "remoting/client/gl_render_layer.h"

namespace {

const int kTextureId = 2;
const float kAnimationDurationMs = 220.f;

}  // namespace

namespace remoting {

GlCursorFeedback::GlCursorFeedback() {}

GlCursorFeedback::~GlCursorFeedback() {}

void GlCursorFeedback::SetCanvas(GlCanvas* canvas) {
  if (!canvas) {
    layer_.reset();
    return;
  }
  layer_.reset(new GlRenderLayer(kTextureId, canvas));
  GlCursorFeedbackTexture* texture = GlCursorFeedbackTexture::GetInstance();
  layer_->SetTexture(texture->GetTexture().data(),
                     GlCursorFeedbackTexture::kTextureWidth,
                     GlCursorFeedbackTexture::kTextureWidth);
}

void GlCursorFeedback::StartAnimation(int x, int y, float diameter) {
  cursor_x_ = x;
  cursor_y_ = y;
  max_diameter_ = diameter;
  animation_start_time_ = base::TimeTicks::Now();
}

bool GlCursorFeedback::Draw() {
  if (!layer_ || animation_start_time_.is_null()) {
    return false;
  }
  float progress =
      (base::TimeTicks::Now() - animation_start_time_).InMilliseconds() /
      kAnimationDurationMs;
  if (progress > 1) {
    animation_start_time_ = base::TimeTicks();
    return false;
  }
  float diameter = progress * max_diameter_;
  std::array<float, 8> positions;
  FillRectangleVertexPositions(cursor_x_ - diameter / 2,
                               cursor_y_ - diameter / 2,
                               diameter, diameter, &positions);
  layer_->SetVertexPositions(positions);
  layer_->Draw(1.f - progress);
  return true;
}

}  // namespace remoting
