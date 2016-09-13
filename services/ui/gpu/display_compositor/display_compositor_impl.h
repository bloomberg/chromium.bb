// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_IMPL_H_
#define SERVICES_UI_GPU_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_IMPL_H_

#include "services/ui/public/interfaces/gpu/display_compositor.mojom.h"
#include "services/ui/public/interfaces/gpu/display_compositor_host.mojom.h"

namespace ui {
namespace gpu {

class DisplayCompositorImpl : public mojom::DisplayCompositor {
 public:
  DisplayCompositorImpl();
  ~DisplayCompositorImpl() override;

  // mojom::DisplayCompositor implementation.
  void CreateDisplay(int accelerated_widget,
                     mojo::InterfaceRequest<mojom::Display> display,
                     mojom::DisplayHostPtr host,
                     mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
                     mojom::CompositorFrameSinkClientPtr client) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayCompositorImpl);
};

}  // namespace gpu
}  // namespace ui

#endif  // SERVICES_UI_GPU_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_IMPL_H_
