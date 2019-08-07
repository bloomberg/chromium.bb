// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DISPLAY_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DISPLAY_PROVIDER_H_

#include <memory>

#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/viz/privileged/interfaces/compositing/display_private.mojom.h"

namespace viz {

class BeginFrameSource;
class Display;
class FrameSinkId;
class RendererSettings;

// Handles creating Display and related classes for FrameSinkManagerImpl.
class DisplayProvider {
 public:
  virtual ~DisplayProvider() {}

  // Creates a new Display for |surface_handle| with |frame_sink_id|. If
  // creating a Display fails this function will return null.
  virtual std::unique_ptr<Display> CreateDisplay(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      BeginFrameSource* begin_frame_source,
      UpdateVSyncParametersCallback update_vsync_callback,
      const RendererSettings& renderer_settings,
      bool send_swap_size_notifications) = 0;

  // Returns an ID that changes on each GPU process restart. This ID can be used
  // for creating unique BeginFrameSource ids.
  virtual uint32_t GetRestartId() const = 0;
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DISPLAY_PROVIDER_H_
