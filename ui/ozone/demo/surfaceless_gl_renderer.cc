// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/surfaceless_gl_renderer.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

namespace {

OverlaySurfaceCandidate MakeOverlayCandidate(int z_order,
                                             gfx::Rect bounds_rect,
                                             gfx::RectF crop_rect) {
  // The overlay checking interface is designed to satisfy the needs of CC which
  // will be producing RectF target rectangles. But we use the bounds produced
  // in RenderFrame for GLSurface::ScheduleOverlayPlane.
  gfx::RectF display_rect(bounds_rect.x(), bounds_rect.y(), bounds_rect.width(),
                          bounds_rect.height());

  OverlaySurfaceCandidate overlay_candidate;

  // Use default display format since this should be compatible with most
  // devices.
  overlay_candidate.format = display::DisplaySnapshot::PrimaryFormat();

  // The bounds rectangle of the candidate overlay buffer.
  overlay_candidate.buffer_size = bounds_rect.size();
  // The same rectangle in floating point coordinates.
  overlay_candidate.display_rect = display_rect;

  // Show the entire buffer by setting the crop to a unity square.
  overlay_candidate.crop_rect = gfx::RectF(0, 0, 1, 1);

  // The demo overlay instance is always ontop and not clipped. Clipped quads
  // cannot be placed in overlays.
  overlay_candidate.is_clipped = false;

  return overlay_candidate;
}

}  // namespace

SurfacelessGlRenderer::BufferWrapper::BufferWrapper() {}

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
      new gl::GLImageNativePixmap(size, GL_BGRA_EXT));
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
    : RendererBase(widget, size),
      overlay_checker_(ui::OzonePlatform::GetInstance()
                           ->GetOverlayManager()
                           ->CreateOverlayCandidates(widget)),
      surface_(surface),
      weak_ptr_factory_(this) {}

SurfacelessGlRenderer::~SurfacelessGlRenderer() {
  // Need to make current when deleting the framebuffer resources allocated in
  // the buffers.
  context_->MakeCurrent(surface_.get());
  for (size_t i = 0; i < base::size(buffers_); ++i)
    buffers_[i].reset();

  for (size_t i = 0; i < base::size(overlay_buffers_); ++i)
    overlay_buffers_[i].reset();
}

bool SurfacelessGlRenderer::Initialize() {
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create GL context";
    return false;
  }

  surface_->Resize(size_, 1.f, gl::GLSurface::ColorSpace::UNSPECIFIED, true);

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make GL context current";
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("partial-primary-plane"))
    primary_plane_rect_ = gfx::Rect(200, 200, 800, 800);
  else
    primary_plane_rect_ = gfx::Rect(size_);

  for (size_t i = 0; i < base::size(buffers_); ++i) {
    buffers_[i].reset(new BufferWrapper());
    if (!buffers_[i]->Initialize(widget_, primary_plane_rect_.size()))
      return false;
  }

  if (command_line->HasSwitch("enable-overlay")) {
    gfx::Size overlay_size = gfx::Size(size_.width() / 8, size_.height() / 8);
    for (size_t i = 0; i < base::size(overlay_buffers_); ++i) {
      overlay_buffers_[i].reset(new BufferWrapper());
      overlay_buffers_[i]->Initialize(gfx::kNullAcceleratedWidget,
                                      overlay_size);

      glViewport(0, 0, overlay_size.width(), overlay_size.height());
      glClearColor(i, 1.0, 0.0, 1.0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      // Ensure that the rendering has been committed to the buffer and thus
      // that the buffer is ready for display without additional
      // synchronization. This allows us to avoid using fences for display
      // synchronization of the non-overlay buffers in RenderFrame.
      glFinish();
    }
  }

  disable_primary_plane_ = command_line->HasSwitch("disable-primary-plane");

  use_gpu_fences_ = command_line->HasSwitch("use-gpu-fences");
  // The GLSurface needs to be prepared to accept plane fences,
  // otherwise any fences sent to it will be ignored.
  if (use_gpu_fences_)
    surface_->SetUsePlaneGpuFences();

  // Schedule the initial render.
  PostRenderFrameTask(gfx::SwapResult::SWAP_ACK);
  return true;
}

void SurfacelessGlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SurfacelessGlRenderer::RenderFrame");

  float fraction = NextFraction();

  gfx::Rect overlay_rect;

  OverlayCandidatesOzone::OverlaySurfaceCandidateList overlay_list;
  if (!disable_primary_plane_) {
    overlay_list.push_back(
        MakeOverlayCandidate(1, gfx::Rect(size_), gfx::RectF(0, 0, 1, 1)));
    // We know at least the primary plane can be scanned out.
    overlay_list.back().overlay_handled = true;
  }
  if (overlay_buffers_[0]) {
    overlay_rect = gfx::Rect(overlay_buffers_[0]->size());

    float steps_num = 5.0f;
    float stepped_fraction =
        std::floor((fraction + 0.5f / steps_num) * steps_num) / steps_num;
    gfx::Vector2d offset(
        stepped_fraction * (size_.width() - overlay_rect.width()),
        (size_.height() - overlay_rect.height()) / 2);
    overlay_rect += offset;
    overlay_list.push_back(
        MakeOverlayCandidate(1, overlay_rect, gfx::RectF(0, 0, 1, 1)));
  }

  // The actual validation for a specific overlay configuration is done
  // asynchronously and then cached inside overlay_checker_ once a reply
  // is sent back.
  // This means that the first few frames we call this method for a specific
  // overlay_list, all the overlays but the primary plane, that we explicitly
  // marked as handled, will be rejected even if they might be handled at a
  // later time.
  overlay_checker_->CheckOverlaySupport(&overlay_list);

  context_->MakeCurrent(surface_.get());
  buffers_[back_buffer_]->BindFramebuffer();

  glViewport(0, 0, size_.width(), size_.height());
  glClearColor(1 - fraction, 0.0, fraction, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!disable_primary_plane_) {
    CHECK(overlay_list.front().overlay_handled);

    // Optionally use a fence to synchronize overlay plane display, if
    // requested when invoking ozone_demo. Note that currently only the primary
    // plane needs to use a fence, since its buffers are dynamically updated
    // every frame. The buffers for non-primary planes are only drawn to during
    // initialization and guaranteed to be ready for display (see Initialize),
    // so no additional fence synchronization is needed for them.
    std::unique_ptr<gl::GLFence> gl_fence =
        use_gpu_fences_ ? gl::GLFence::CreateForGpuFence() : nullptr;

    surface_->ScheduleOverlayPlane(
        0, gfx::OVERLAY_TRANSFORM_NONE, buffers_[back_buffer_]->image(),
        primary_plane_rect_, gfx::RectF(0, 0, 1, 1), false,
        gl_fence ? gl_fence->GetGpuFence() : nullptr);
  }

  if (overlay_buffers_[0] && overlay_list.back().overlay_handled) {
    surface_->ScheduleOverlayPlane(1, gfx::OVERLAY_TRANSFORM_NONE,
                                   overlay_buffers_[back_buffer_]->image(),
                                   overlay_rect, gfx::RectF(0, 0, 1, 1), false,
                                   /* gpu_fence */ nullptr);
  }

  back_buffer_ ^= 1;
  surface_->SwapBuffersAsync(
      base::Bind(&SurfacelessGlRenderer::PostRenderFrameTask,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind([](const gfx::PresentationFeedback&) {}));
}

void SurfacelessGlRenderer::PostRenderFrameTask(gfx::SwapResult result) {
  switch (result) {
    case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
      for (size_t i = 0; i < base::size(buffers_); ++i) {
        buffers_[i].reset(new BufferWrapper());
        if (!buffers_[i]->Initialize(widget_, primary_plane_rect_.size()))
          LOG(FATAL) << "Failed to recreate buffer";
      }
      FALLTHROUGH;  // We want to render a new frame anyways.
    case gfx::SwapResult::SWAP_ACK:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&SurfacelessGlRenderer::RenderFrame,
                                weak_ptr_factory_.GetWeakPtr()));
      break;
    case gfx::SwapResult::SWAP_FAILED:
      LOG(FATAL) << "Failed to swap buffers";
      break;
  }
}

void SurfacelessGlRenderer::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  DCHECK(surface_->SupportsPresentationCallback());
  LOG_IF(ERROR, feedback.timestamp.is_null()) << "Last frame is discarded!";
}

}  // namespace ui
