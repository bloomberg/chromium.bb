// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CompositorFrame;
class CopyOutputRequest;
class RenderPass;
enum class SurfaceDrawStatus;
}

namespace ui {

class DisplayCompositor;
class GpuState;
class SurfacesState;

namespace ws {

namespace test {
class FrameGeneratorTest;
}

class FrameGeneratorDelegate;
class ServerWindow;

// Responsible for redrawing the display in response to the redraw requests by
// submitting CompositorFrames to the owned DisplayCompositor.
class FrameGenerator {
 public:
  FrameGenerator(FrameGeneratorDelegate* delegate,
                 scoped_refptr<GpuState> gpu_state,
                 scoped_refptr<SurfacesState> surfaces_state);
  virtual ~FrameGenerator();

  // Schedules a redraw for the provided region.
  void RequestRedraw(const gfx::Rect& redraw_region);
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget);
  void RequestCopyOfOutput(
      std::unique_ptr<cc::CopyOutputRequest> output_request);

  bool is_frame_pending() { return frame_pending_; }

 private:
  friend class ui::ws::test::FrameGeneratorTest;

  void WantToDraw();

  // This method initiates a top level redraw of the display.
  // TODO(fsamuel): This should use vblank as a signal rather than a timer
  // http://crbug.com/533042
  void Draw();

  // This is called after the DisplayCompositor has completed generating a new
  // frame for the display. TODO(fsamuel): Idle time processing should happen
  // here if there is budget for it.
  void DidDraw(cc::SurfaceDrawStatus status);

  // Generates the CompositorFrame for the current |dirty_rect_|.
  cc::CompositorFrame GenerateCompositorFrame();

  // DrawWindowTree recursively visits ServerWindows, creating a SurfaceDrawQuad
  // for each that lacks one.
  void DrawWindowTree(cc::RenderPass* pass,
                      ServerWindow* window,
                      const gfx::Vector2d& parent_to_root_origin_offset,
                      float opacity);

  FrameGeneratorDelegate* delegate_;
  scoped_refptr<GpuState> gpu_state_;
  scoped_refptr<SurfacesState> surfaces_state_;

  std::unique_ptr<DisplayCompositor> display_compositor_;

  // The region that needs to be redrawn next time the compositor frame is
  // generated.
  gfx::Rect dirty_rect_;
  base::Timer draw_timer_;
  bool frame_pending_ = false;

  base::WeakPtrFactory<FrameGenerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
