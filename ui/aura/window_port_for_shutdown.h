// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PORT_FOR_SHUTDOWN_H_
#define UI_AURA_WINDOW_PORT_FOR_SHUTDOWN_H_

#include "ui/aura/window_port.h"

#include "components/viz/common/surfaces/local_surface_id.h"

namespace aura {

// When WindowTreeClient is destroyed any existing windows get a
// WindowPortForShutdown assigned to them. This allows for the Window to keep
// working without a WindowTreeClient. This class is *only* used inside aura.
class WindowPortForShutdown : public WindowPort {
 public:
  WindowPortForShutdown();
  ~WindowPortForShutdown() override;

  static void Install(aura::Window* window);

  // WindowPort:
  void OnPreInit(Window* window) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWillAddChild(Window* child) override;
  void OnWillRemoveChild(Window* child) override;
  void OnWillMoveChild(size_t current_index, size_t dest_index) override;
  void OnVisibilityChanged(bool visible) override;
  void OnDidChangeBounds(const gfx::Rect& old_bounds,
                         const gfx::Rect& new_bounds) override;
  void OnDidChangeTransform(const gfx::Transform& old_transform,
                            const gfx::Transform& new_transform) override;
  std::unique_ptr<ui::PropertyData> OnWillChangeProperty(
      const void* key) override;
  void OnPropertyChanged(const void* key,
                         int64_t old_value,
                         std::unique_ptr<ui::PropertyData> data) override;
  std::unique_ptr<cc::LayerTreeFrameSink> CreateLayerTreeFrameSink() override;
  viz::SurfaceId GetSurfaceId() const override;
  void AllocateLocalSurfaceId() override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() override;
  viz::FrameSinkId GetFrameSinkId() const override;
  void OnWindowAddedToRootWindow() override;
  void OnWillRemoveWindowFromRootWindow() override;
  void OnEventTargetingPolicyChanged() override;

 private:
  viz::LocalSurfaceId local_surface_id_;
  viz::FrameSinkId frame_sink_id_;
  DISALLOW_COPY_AND_ASSIGN(WindowPortForShutdown);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_PORT_FOR_SHUTDOWN_H_
