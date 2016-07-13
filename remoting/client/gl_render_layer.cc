// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/gl_render_layer.h"

#include "base/logging.h"
#include "remoting/client/gl_canvas.h"
#include "remoting/client/gl_helpers.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace {

// Assign texture coordinates to buffers for use in shader program.
const float kVertices[] = {
    // Points order: upper-left, bottom-left, upper-right, bottom-right.

    // Positions to draw the texture on the normalized canvas coordinate.
    0, 0, 0, 1, 1, 0, 1, 1,

    // Region of the texture to be used (normally the whole texture).
    0, 0, 0, 1, 1, 0, 1, 1};

const int kDefaultUpdateBufferCapacity =
    2048 * 2048 * webrtc::DesktopFrame::kBytesPerPixel;

}

namespace remoting {

GlRenderLayer::GlRenderLayer(int texture_id, GlCanvas* canvas)
    : texture_id_(texture_id), canvas_(canvas) {
  texture_handle_ = CreateTexture();
  buffer_handle_ = CreateBuffer(kVertices, sizeof(kVertices));
}

GlRenderLayer::~GlRenderLayer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  glDeleteBuffers(1, &buffer_handle_);
  glDeleteTextures(1, &texture_handle_);
}

void GlRenderLayer::SetTexture(const uint8_t* texture, int width, int height) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(width > 0 && height > 0);
  texture_set_ = true;
  glActiveTexture(GL_TEXTURE0 + texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_handle_);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void PackDirtyRegion(uint8_t* dest,
                     const uint8_t* source,
                     int width,
                     int height,
                     int stride) {
  for (int i = 0; i < height; i++) {
    memcpy(dest, source, width * webrtc::DesktopFrame::kBytesPerPixel);
    source += stride;
    dest += webrtc::DesktopFrame::kBytesPerPixel * width;
  }
}

void GlRenderLayer::UpdateTexture(const uint8_t* subtexture,
                                  int offset_x,
                                  int offset_y,
                                  int width,
                                  int height,
                                  int stride) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(texture_set_);
  DCHECK(width > 0 && height > 0);
  glActiveTexture(GL_TEXTURE0 + texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_handle_);

  bool stride_multiple_of_bytes_per_pixel =
      stride % webrtc::DesktopFrame::kBytesPerPixel == 0;
  bool loosely_packed =
      !stride_multiple_of_bytes_per_pixel ||
      (stride > 0 && stride != webrtc::DesktopFrame::kBytesPerPixel * width);

  const void* buffer_to_update = subtexture;

  if (loosely_packed) {
    if (stride_multiple_of_bytes_per_pixel && canvas_->GetGlVersion() >= 3) {
      glPixelStorei(GL_UNPACK_ROW_LENGTH,
                    stride / webrtc::DesktopFrame::kBytesPerPixel);
    } else {
      // Doesn't support GL_UNPACK_ROW_LENGTH or stride not multiple of
      // kBytesPerPixel. Manually pack the data.
      int required_size = width * height * webrtc::DesktopFrame::kBytesPerPixel;
      if (update_buffer_size_ < required_size) {
        if (required_size < kDefaultUpdateBufferCapacity) {
          update_buffer_size_ = kDefaultUpdateBufferCapacity;
        } else {
          update_buffer_size_ = required_size;
        }
        update_buffer_.reset(new uint8_t[update_buffer_size_]);
      }
      PackDirtyRegion(update_buffer_.get(), subtexture, width, height, stride);
      buffer_to_update = update_buffer_.get();
    }
  }

  glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, GL_RGBA,
                  GL_UNSIGNED_BYTE, buffer_to_update);

  if (loosely_packed && stride_multiple_of_bytes_per_pixel &&
      canvas_->GetGlVersion() >= 3) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GlRenderLayer::SetVertexPositions(const std::array<float, 8>& positions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  glBindBuffer(GL_ARRAY_BUFFER, buffer_handle_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(kVertices) / 2, positions.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlRenderLayer::SetTextureVisibleArea(
    const std::array<float, 8>& positions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  glBindBuffer(GL_ARRAY_BUFFER, buffer_handle_);
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(kVertices) / 2, sizeof(kVertices) / 2,
                  positions.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlRenderLayer::Draw() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(texture_set_);
  canvas_->DrawTexture(texture_id_, texture_handle_, buffer_handle_);
}

}  // namespace remoting
