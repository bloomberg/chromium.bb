// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gl_desktop.h"

#include "base/logging.h"
#include "remoting/client/gl_canvas.h"
#include "remoting/client/gl_render_layer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace {

const int kTextureId = 0;

}  // namespace

namespace remoting {

GlDesktop::GlDesktop() {}

GlDesktop::~GlDesktop() {}

void GlDesktop::SetCanvas(GlCanvas* canvas) {
  if (!canvas) {
    layer_.reset();
    return;
  }
  layer_.reset(new GlRenderLayer(kTextureId, canvas));
  last_desktop_size_.set(0, 0);
}

void GlDesktop::SetVideoFrame(std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (!layer_) {
    return;
  }
  if (!frame->size().equals(last_desktop_size_)) {
    layer_->SetTexture(frame->data(), frame->size().width(),
                       frame->size().height());
    last_desktop_size_.set(frame->size().width(), frame->size().height());
  } else {
    for (webrtc::DesktopRegion::Iterator i(frame->updated_region());
        !i.IsAtEnd(); i.Advance()) {
      const uint8_t* rect_start =
          frame->GetFrameDataAtPos(i.rect().top_left());
      layer_->UpdateTexture(
          rect_start, i.rect().left(), i.rect().top(), i.rect().width(),
          i.rect().height(), frame->stride());
    }
  }
}

void GlDesktop::Draw() {
  if (layer_ && !last_desktop_size_.is_empty()) {
    layer_->Draw(1.f);
  }
}

}  // namespace remoting
