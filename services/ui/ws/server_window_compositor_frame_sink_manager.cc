// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {
namespace ws {

ServerWindowCompositorFrameSinkManager::ServerWindowCompositorFrameSinkManager(
    ServerWindow* window)
    : window_(window),
      waiting_for_initial_frames_(
          window_->properties().count(ui::mojom::kWaitForUnderlay_Property) >
          0) {}

ServerWindowCompositorFrameSinkManager::
    ~ServerWindowCompositorFrameSinkManager() {
  // Explicitly clear the type to surface manager so that this manager
  // is still valid prior during ~ServerWindowCompositorFrameSink.
  type_to_compositor_frame_sink_map_.clear();
}

bool ServerWindowCompositorFrameSinkManager::ShouldDraw() {
  if (!waiting_for_initial_frames_)
    return true;

  waiting_for_initial_frames_ = !IsCompositorFrameSinkReadyAndNonEmpty(
                                    mojom::CompositorFrameSinkType::DEFAULT) ||
                                !IsCompositorFrameSinkReadyAndNonEmpty(
                                    mojom::CompositorFrameSinkType::UNDERLAY);
  return !waiting_for_initial_frames_;
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    gfx::AcceleratedWidget widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<SurfacesContextProvider> context_provider,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  cc::FrameSinkId frame_sink_id(
      WindowIdToTransportId(window_->id()),
      static_cast<uint32_t>(compositor_frame_sink_type));
  CompositorFrameSinkData& data =
      type_to_compositor_frame_sink_map_[compositor_frame_sink_type];
  // TODO(fsamuel): Create the CompositorFrameSink through the DisplayCompositor
  // mojo interface and hold on to a MojoCompositorFrameSinkPtr.
  data.compositor_frame_sink =
      base::MakeUnique<ServerWindowCompositorFrameSink>(
          this, frame_sink_id, widget, gpu_memory_buffer_manager,
          std::move(context_provider), std::move(request), std::move(client));
  data.surface_sequence_generator.set_frame_sink_id(frame_sink_id);
}

ServerWindowCompositorFrameSink*
ServerWindowCompositorFrameSinkManager::GetDefaultCompositorFrameSink() const {
  return GetCompositorFrameSinkByType(mojom::CompositorFrameSinkType::DEFAULT);
}

ServerWindowCompositorFrameSink*
ServerWindowCompositorFrameSinkManager::GetUnderlayCompositorFrameSink() const {
  return GetCompositorFrameSinkByType(mojom::CompositorFrameSinkType::UNDERLAY);
}

ServerWindowCompositorFrameSink*
ServerWindowCompositorFrameSinkManager::GetCompositorFrameSinkByType(
    mojom::CompositorFrameSinkType type) const {
  auto iter = type_to_compositor_frame_sink_map_.find(type);
  return iter == type_to_compositor_frame_sink_map_.end()
             ? nullptr
             : iter->second.compositor_frame_sink.get();
}

bool ServerWindowCompositorFrameSinkManager::HasCompositorFrameSinkOfType(
    mojom::CompositorFrameSinkType type) const {
  return type_to_compositor_frame_sink_map_.count(type) > 0;
}

bool ServerWindowCompositorFrameSinkManager::HasAnyCompositorFrameSink() const {
  return GetDefaultCompositorFrameSink() || GetUnderlayCompositorFrameSink();
}

cc::SurfaceSequence
ServerWindowCompositorFrameSinkManager::CreateSurfaceSequence(
    mojom::CompositorFrameSinkType type) {
  cc::FrameSinkId frame_sink_id(WindowIdToTransportId(window_->id()),
                                static_cast<uint32_t>(type));
  CompositorFrameSinkData& data = type_to_compositor_frame_sink_map_[type];
  data.surface_sequence_generator.set_frame_sink_id(frame_sink_id);
  return data.surface_sequence_generator.CreateSurfaceSequence();
}

gfx::Size ServerWindowCompositorFrameSinkManager::GetLatestFrameSize(
    mojom::CompositorFrameSinkType type) const {
  auto it = type_to_compositor_frame_sink_map_.find(type);
  if (it == type_to_compositor_frame_sink_map_.end())
    return gfx::Size();

  return it->second.latest_submitted_frame_size;
}

cc::SurfaceId ServerWindowCompositorFrameSinkManager::GetLatestSurfaceId(
    mojom::CompositorFrameSinkType type) const {
  auto it = type_to_compositor_frame_sink_map_.find(type);
  if (it == type_to_compositor_frame_sink_map_.end())
    return cc::SurfaceId();

  return it->second.latest_submitted_surface_id;
}

void ServerWindowCompositorFrameSinkManager::SetLatestSurfaceInfo(
    mojom::CompositorFrameSinkType type,
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size) {
  CompositorFrameSinkData& data = type_to_compositor_frame_sink_map_[type];
  data.latest_submitted_surface_id = surface_id;
  data.latest_submitted_frame_size = frame_size;
}

cc::SurfaceManager*
ServerWindowCompositorFrameSinkManager::GetCompositorFrameSinkManager() {
  return window()->delegate()->GetDisplayCompositor()->manager();
}

bool ServerWindowCompositorFrameSinkManager::
    IsCompositorFrameSinkReadyAndNonEmpty(
        mojom::CompositorFrameSinkType type) const {
  auto iter = type_to_compositor_frame_sink_map_.find(type);
  if (iter == type_to_compositor_frame_sink_map_.end())
    return false;
  if (iter->second.latest_submitted_frame_size.IsEmpty())
    return false;
  const gfx::Size& latest_submitted_frame_size =
      iter->second.latest_submitted_frame_size;
  return latest_submitted_frame_size.width() >= window_->bounds().width() &&
         latest_submitted_frame_size.height() >= window_->bounds().height();
}

CompositorFrameSinkData::CompositorFrameSinkData() {}

CompositorFrameSinkData::~CompositorFrameSinkData() {}

CompositorFrameSinkData::CompositorFrameSinkData(
    CompositorFrameSinkData&& other)
    : latest_submitted_surface_id(other.latest_submitted_surface_id),
      compositor_frame_sink(std::move(other.compositor_frame_sink)) {}

CompositorFrameSinkData& CompositorFrameSinkData::operator=(
    CompositorFrameSinkData&& other) {
  latest_submitted_surface_id = other.latest_submitted_surface_id;
  compositor_frame_sink = std::move(other.compositor_frame_sink);
  return *this;
}

}  // namespace ws
}  // namespace ui
