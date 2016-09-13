// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/display_compositor/display_compositor_impl.h"

#include "services/ui/gpu/display_compositor/display_impl.h"

namespace ui {
namespace gpu {

DisplayCompositorImpl::DisplayCompositorImpl() {}

DisplayCompositorImpl::~DisplayCompositorImpl() {}

void DisplayCompositorImpl::CreateDisplay(
    int accelerated_widget,
    mojo::InterfaceRequest<mojom::Display> display,
    mojom::DisplayHostPtr host,
    mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
    mojom::CompositorFrameSinkClientPtr client) {
  NOTIMPLEMENTED();
}

}  // namespace gpu
}  // namespace ui
