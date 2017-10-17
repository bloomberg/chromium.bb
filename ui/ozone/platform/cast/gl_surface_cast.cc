// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/gl_surface_cast.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/cast_features.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/cast/gl_ozone_egl_cast.h"

namespace {
// Target fixed 30fps, or 60fps if doing triple-buffer 720p.
// TODO(halliwell): We might need to customize this value on various devices
// or make it dynamic that throttles framerate if device is overheating.
base::TimeDelta GetVSyncInterval() {
  if (base::FeatureList::IsEnabled(chromecast::kTripleBuffer720)) {
    return base::TimeDelta::FromSeconds(1) / 59.9;
  } else {
    return base::TimeDelta::FromSeconds(2) / 59.9;
  }
}

}  // namespace

namespace ui {

GLSurfaceCast::GLSurfaceCast(gfx::AcceleratedWidget widget,
                             GLOzoneEglCast* parent)
    : NativeViewGLSurfaceEGL(
          parent->GetNativeWindow(),
          std::make_unique<gfx::FixedVSyncProvider>(base::TimeTicks(),
                                                    GetVSyncInterval())),
      widget_(widget),
      parent_(parent),
      supports_swap_buffer_with_bounds_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableSwapBuffersWithBounds)) {
  DCHECK(parent_);
}

bool GLSurfaceCast::SupportsSwapBuffersWithBounds() {
  return supports_swap_buffer_with_bounds_;
}

gfx::SwapResult GLSurfaceCast::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  if (result == gfx::SwapResult::SWAP_ACK)
    parent_->OnSwapBuffers();

  return result;
}

gfx::SwapResult GLSurfaceCast::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects) {
  DCHECK(supports_swap_buffer_with_bounds_);

  // TODO(halliwell): Request new EGL extension so we're not abusing
  // SwapBuffersWithDamage here.
  std::vector<int> rects_data(rects.size() * 4);
  for (size_t i = 0; i != rects.size(); ++i) {
    rects_data[i * 4 + 0] = rects[i].x();
    rects_data[i * 4 + 1] = rects[i].y();
    rects_data[i * 4 + 2] = rects[i].width();
    rects_data[i * 4 + 3] = rects[i].height();
  }
  gfx::SwapResult result =
      NativeViewGLSurfaceEGL::SwapBuffersWithDamage(rects_data);
  if (result == gfx::SwapResult::SWAP_ACK)
    parent_->OnSwapBuffers();
  return result;
}

bool GLSurfaceCast::Resize(const gfx::Size& size,
                           float scale_factor,
                           ColorSpace color_space,
                           bool has_alpha) {
  return parent_->ResizeDisplay(size) &&
         NativeViewGLSurfaceEGL::Resize(size, scale_factor, color_space,
                                        has_alpha);
}

bool GLSurfaceCast::ScheduleOverlayPlane(int z_order,
                                         gfx::OverlayTransform transform,
                                         gl::GLImage* image,
                                         const gfx::Rect& bounds_rect,
                                         const gfx::RectF& crop_rect) {
  return image->ScheduleOverlayPlane(widget_, z_order, transform, bounds_rect,
                                     crop_rect);
}

EGLConfig GLSurfaceCast::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetDisplay(), config_attribs);
  }
  return config_;
}

GLSurfaceCast::~GLSurfaceCast() {
  Destroy();
  parent_->ChildDestroyed();
}

}  // namespace ui
