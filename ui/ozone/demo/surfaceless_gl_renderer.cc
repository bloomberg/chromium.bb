// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/surfaceless_gl_renderer.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

SurfacelessGlRenderer::BufferWrapper::BufferWrapper() {
}

SurfacelessGlRenderer::BufferWrapper::~BufferWrapper() {
  if (gl_fb_)
    glDeleteFramebuffersEXT(1, &gl_fb_);

  if (gl_tex_) {
    image_->ReleaseTexImage(GL_TEXTURE_2D);
    glDeleteTextures(1, &gl_tex_);
  }
}

bool SurfacelessGlRenderer::BufferWrapper::Initialize(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  glGenFramebuffersEXT(1, &gl_fb_);
  glGenTextures(1, &gl_tex_);

  gfx::BufferFormat format = display::DisplaySnapshot::PrimaryFormat();
  scoped_refptr<gfx::NativePixmap> pixmap =
      OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(widget, size, format, gfx::BufferUsage::SCANOUT);
  scoped_refptr<gl::GLImageNativePixmap> image(
      new gl::GLImageNativePixmap(size, GL_RGB));
  if (!image->Initialize(pixmap.get(), format)) {
    LOG(ERROR) << "Failed to create GLImage";
    return false;
  }
  image_ = image;

  glBindFramebufferEXT(GL_FRAMEBUFFER, gl_fb_);
  glBindTexture(GL_TEXTURE_2D, gl_tex_);
  image_->BindTexImage(GL_TEXTURE_2D);

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            gl_tex_, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Failed to create framebuffer "
               << glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    return false;
  }

  widget_ = widget;
  size_ = size;

  return true;
}

void SurfacelessGlRenderer::BufferWrapper::BindFramebuffer() {
  glBindFramebufferEXT(GL_FRAMEBUFFER, gl_fb_);
}

SurfacelessGlRenderer::SurfacelessGlRenderer(
    gfx::AcceleratedWidget widget,
    const scoped_refptr<gl::GLSurface>& surface,
    const gfx::Size& size)
    : GlRenderer(widget, surface, size),
      overlay_checker_(ui::OzonePlatform::GetInstance()
                           ->GetOverlayManager()
                           ->CreateOverlayCandidates(widget)),
      weak_ptr_factory_(this) {}

SurfacelessGlRenderer::~SurfacelessGlRenderer() {
  // Need to make current when deleting the framebuffer resources allocated in
  // the buffers.
  context_->MakeCurrent(surface_.get());
}

bool SurfacelessGlRenderer::Initialize() {
  if (!GlRenderer::Initialize())
    return false;

  for (size_t i = 0; i < arraysize(buffers_); ++i) {
    buffers_[i].reset(new BufferWrapper());
    if (!buffers_[i]->Initialize(widget_, size_))
      return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("enable-overlay")) {
    gfx::Size overlay_size = gfx::Size(size_.width() / 8, size_.height() / 8);
    overlay_buffer_.reset(new BufferWrapper());
    overlay_buffer_->Initialize(gfx::kNullAcceleratedWidget, overlay_size);

    glViewport(0, 0, overlay_size.width(), overlay_size.height());
    glClearColor(1.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  PostRenderFrameTask(gfx::SwapResult::SWAP_ACK);
  return true;
}

// OverlayChecker demonstrates how to use the
// OverlayCandidatesOzone::CheckOverlaySupport to determine if a given overlay
// can be successfully displayed.
void SurfacelessGlRenderer::OverlayChecker(int z_order,
                                           gfx::Rect bounds_rect,
                                           gfx::RectF crop_rect) {
  // The overlay checking interface is designed to satisfy the needs of CC which
  // will be producing RectF target rectangles. But we use the bounds produced
  // in RenderFrame for GLSurface::ScheduleOverlayPlane.
  gfx::RectF display_rect(bounds_rect.x(), bounds_rect.y(), bounds_rect.width(),
                          bounds_rect.height());

  OverlayCandidatesOzone::OverlaySurfaceCandidate overlay_candidate;

  // The bounds rectangle of the candidate overlay buffer.
  overlay_candidate.buffer_size = bounds_rect.size();
  // The same rectangle in floating point coordinates.
  overlay_candidate.display_rect = display_rect;

  // Show the entire buffer by setting the crop to a unity square.
  overlay_candidate.crop_rect = gfx::RectF(0, 0, 1, 1);

  // The quad rect is the rectangular bounds of the overlay demonstration
  // rectangle.
  overlay_candidate.quad_rect_in_target_space = bounds_rect;

  // The clip region is the entire screen.
  overlay_candidate.clip_rect = gfx::Rect(size_);

  // The demo overlay instance is always ontop and not clipped. Clipped quads
  // cannot be placed in overlays.
  overlay_candidate.is_clipped = false;

  OverlayCandidatesOzone::OverlaySurfaceCandidateList list;
  list.push_back(overlay_candidate);

  // Ask ozone platform to determine if this rect can be placed in an overlay.
  // Ozone will update the list and return it.
  overlay_checker_->CheckOverlaySupport(&list);

  // Note what the checker decided.
  // say more about it.
  TRACE_EVENT2("hwoverlays", "SurfacelessGlRenderer::OverlayChecker", "canihaz",
               list[0].overlay_handled, "display_rect",
               list[0].display_rect.ToString());
}

void SurfacelessGlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SurfacelessGlRenderer::RenderFrame");

  float fraction = NextFraction();

  context_->MakeCurrent(surface_.get());
  buffers_[back_buffer_]->BindFramebuffer();

  glViewport(0, 0, size_.width(), size_.height());
  glClearColor(1 - fraction, 0.0, fraction, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  surface_->ScheduleOverlayPlane(0, gfx::OVERLAY_TRANSFORM_NONE,
                                 buffers_[back_buffer_]->image(),
                                 gfx::Rect(size_), gfx::RectF(0, 0, 1, 1));

  if (overlay_buffer_) {
    gfx::Rect overlay_rect(overlay_buffer_->size());
    gfx::Vector2d offset(fraction * (size_.width() - overlay_rect.width()),
                         (size_.height() - overlay_rect.height()) / 2);
    overlay_rect += offset;

    // TODO(rjkroege): Overlay checking should gate the following and will
    // be added after solving http://crbug.com/735640
    OverlayChecker(1, overlay_rect, gfx::RectF(0, 0, 1, 1));

    surface_->ScheduleOverlayPlane(1, gfx::OVERLAY_TRANSFORM_NONE,
                                   overlay_buffer_->image(), overlay_rect,
                                   gfx::RectF(0, 0, 1, 1));
  }

  back_buffer_ ^= 1;
  surface_->SwapBuffersAsync(
      base::Bind(&SurfacelessGlRenderer::PostRenderFrameTask,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SurfacelessGlRenderer::PostRenderFrameTask(gfx::SwapResult result) {
  switch (result) {
    case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
      for (size_t i = 0; i < arraysize(buffers_); ++i) {
        buffers_[i].reset(new BufferWrapper());
        if (!buffers_[i]->Initialize(widget_, size_))
          LOG(FATAL) << "Failed to recreate buffer";
      }
    // Fall through since we want to render a new frame anyways.
    case gfx::SwapResult::SWAP_ACK:
      GlRenderer::PostRenderFrameTask(result);
      break;
    case gfx::SwapResult::SWAP_FAILED:
      LOG(FATAL) << "Failed to swap buffers";
      break;
  }
}

}  // namespace ui
