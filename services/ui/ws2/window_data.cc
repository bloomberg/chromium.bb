// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_data.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ui/ws2/window_host_frame_sink_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::ws2::WindowData*);

namespace ui {
namespace ws2 {
namespace {
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ws2::WindowData,
                                   kWindowDataKey,
                                   nullptr);
}  // namespace

WindowData::~WindowData() = default;

// static
WindowData* WindowData::Create(aura::Window* window,
                               WindowServiceClient* client,
                               const viz::FrameSinkId& frame_sink_id) {
  DCHECK(!GetMayBeNull(window));
  // Owned by |window|.
  WindowData* data = new WindowData(window, client, frame_sink_id);
  window->SetProperty(kWindowDataKey, data);
  return data;
}

// static
const WindowData* WindowData::GetMayBeNull(const aura::Window* window) {
  return window->GetProperty(kWindowDataKey);
}

void WindowData::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  if (!window_host_frame_sink_client_) {
    window_host_frame_sink_client_ =
        std::make_unique<WindowHostFrameSinkClient>();
    host_frame_sink_manager->RegisterFrameSinkId(
        frame_sink_id_, window_host_frame_sink_client_.get());
    // TODO: need to unregister, and update on changes.
    // TODO: figure what parts of the ancestors (and descendants) need to be
    // registered.
  }

  host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id, window_host_frame_sink_client_.get());
  window_->SetEmbedFrameSinkId(frame_sink_id_);
  window_host_frame_sink_client_->OnFrameSinkIdChanged();
}

void WindowData::AttachCompositorFrameSink(
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(compositor_frame_sink), std::move(client));
}

WindowData::WindowData(aura::Window* window,
                       WindowServiceClient* client,
                       const viz::FrameSinkId& frame_sink_id)
    : window_(window),
      owning_window_service_client_(client),
      frame_sink_id_(frame_sink_id) {}

}  // namespace ws2
}  // namespace ui
