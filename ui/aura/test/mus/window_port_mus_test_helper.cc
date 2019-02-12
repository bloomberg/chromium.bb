// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/window_port_mus_test_helper.h"

#include "ui/aura/mus/mus_lsi_allocator.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window.h"

namespace aura {

// Start with something large that shouldn't conflict with any other values.
// static
uint32_t WindowPortMusTestHelper::next_client_id_ = 10001;

WindowPortMusTestHelper::WindowPortMusTestHelper(Window* window)
    : window_port_mus_(WindowPortMus::Get(window)) {
  DCHECK(window_port_mus_);
}

WindowPortMusTestHelper::~WindowPortMusTestHelper() = default;

void WindowPortMusTestHelper::SimulateEmbedding() {
  window_port_mus_->PrepareForEmbed();
  window_port_mus_->SetFrameSinkIdFromServer(
      viz::FrameSinkId(next_client_id_++, 1));
}

base::WeakPtr<cc::LayerTreeFrameSink> WindowPortMusTestHelper::GetFrameSink() {
  return window_port_mus_->local_layer_tree_frame_sink_;
}

ClientSurfaceEmbedder* WindowPortMusTestHelper::GetClientSurfaceEmbedder() {
  return GetParentAllocator()
             ? GetParentAllocator()->client_surface_embedder_.get()
             : nullptr;
}

viz::ParentLocalSurfaceIdAllocator*
WindowPortMusTestHelper::GetParentLocalSurfaceIdAllocator() {
  return GetParentAllocator()
             ? &(GetParentAllocator()->parent_local_surface_id_allocator_)
             : nullptr;
}

MusLsiAllocator* WindowPortMusTestHelper::GetAllocator() {
  return window_port_mus_->allocator_.get();
}

ParentAllocator* WindowPortMusTestHelper::GetParentAllocator() {
  return window_port_mus_->allocator_
             ? static_cast<ParentAllocator*>(window_port_mus_->allocator_.get())
             : nullptr;
}

}  // namespace aura
