// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gl_surface_egl_readback.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/common/egl_util.h"

namespace ui {
namespace {

constexpr size_t kBytesPerPixelBGRA = 4;

}  // namespace

GLSurfaceEglReadback::GLSurfaceEglReadback()
    : PbufferGLSurfaceEGL(gfx::Size(1, 1)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

bool GLSurfaceEglReadback::Resize(const gfx::Size& size,
                                  float scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  bool has_alpha) {
  pixels_.reset();

  if (!PbufferGLSurfaceEGL::Resize(size, scale_factor, color_space, has_alpha))
    return false;

  // Allocate a new buffer for readback.
  const size_t buffer_size = size.width() * size.height() * kBytesPerPixelBGRA;
  pixels_ = std::make_unique<uint8_t[]>(buffer_size);

  return true;
}

bool GLSurfaceEglReadback::IsOffscreen() {
  return false;
}

gfx::SwapResult GLSurfaceEglReadback::SwapBuffers(
    PresentationCallback callback) {
  ReadPixels(pixels_.get());

  gfx::SwapResult swap_result = gfx::SwapResult::SWAP_FAILED;
  gfx::PresentationFeedback feedback;

  if (HandlePixels(pixels_.get())) {
    // Swap is succesful, so return SWAP_ACK and provide the current time with
    // presentation feedback.
    swap_result = gfx::SwapResult::SWAP_ACK;
    feedback.timestamp = base::TimeTicks::Now();
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), feedback));
  return swap_result;
}

gfx::SurfaceOrigin GLSurfaceEglReadback::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

GLSurfaceEglReadback::~GLSurfaceEglReadback() {
  Destroy();
}

bool GLSurfaceEglReadback::HandlePixels(uint8_t* pixels) {
  return true;
}

void GLSurfaceEglReadback::ReadPixels(void* buffer) {
  const gfx::Size size = GetSize();

  GLint read_fbo = 0;
  GLint pixel_pack_buffer = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pixel_pack_buffer);

  // Make sure pixels are read from fbo 0.
  if (read_fbo)
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);

  // Make sure pixels are stored into |pixels_| instead of buffer binding to
  // GL_PIXEL_PACK_BUFFER.
  if (pixel_pack_buffer)
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  glReadPixels(0, 0, size.width(), size.height(), GL_BGRA, GL_UNSIGNED_BYTE,
               buffer);

  if (read_fbo)
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, read_fbo);
  if (pixel_pack_buffer)
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pixel_pack_buffer);
}

}  // namespace ui
