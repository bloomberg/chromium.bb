// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/software_renderer.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

SoftwareRenderer::SoftwareRenderer(gfx::AcceleratedWidget widget,
                                   const gfx::Size& size)
    : RendererBase(widget, size) {
}

SoftwareRenderer::~SoftwareRenderer() {
}

bool SoftwareRenderer::Initialize() {
  software_surface_ =
      ui::SurfaceFactoryOzone::GetInstance()->CreateCanvasForWidget(widget_);
  if (!software_surface_) {
    LOG(ERROR) << "Failed to create software surface";
    return false;
  }

  software_surface_->ResizeCanvas(size_);
  return true;
}

void SoftwareRenderer::RenderFrame() {
  float fraction = NextFraction();

  skia::RefPtr<SkSurface> surface = software_surface_->GetSurface();

  SkColor color =
      SkColorSetARGB(0xff, 0, 0xff * fraction, 0xff * (1 - fraction));

  surface->getCanvas()->clear(color);

  software_surface_->PresentCanvas(gfx::Rect(size_));
}

}  // namespace ui
