// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_

#include <fuchsia/ui/input/cpp/fidl.h>
#include <fuchsia/ui/viewsv1/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/events/fuchsia/input_event_dispatcher.h"
#include "ui/events/fuchsia/input_event_dispatcher_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class ScenicWindowManager;
class PlatformWindowDelegate;

class OZONE_EXPORT ScenicWindow : public PlatformWindow,
                                  public fuchsia::ui::viewsv1::ViewListener,
                                  public fuchsia::ui::input::InputListener,
                                  public InputEventDispatcherDelegate {
 public:
  // Both |window_manager| and |delegate| must outlive the ScenicWindow.
  // |view_token| is passed to Scenic to attach the view to the view tree.
  ScenicWindow(ScenicWindowManager* window_manager,
               PlatformWindowDelegate* delegate,
               zx::eventpair view_token);
  ~ScenicWindow() override;

  scenic::Session* scenic_session() { return &scenic_session_; }

  void ExportRenderingEntity(zx::eventpair export_token);

  // PlatformWindow implementation.
  gfx::Rect GetBounds() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetTitle(const base::string16& title) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  void PrepareForShutdown() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;

 private:
  // views::ViewListener interface.
  void OnPropertiesChanged(fuchsia::ui::viewsv1::ViewProperties properties,
                           OnPropertiesChangedCallback callback) override;

  // fuchsia::ui::input::InputListener interface.
  // TODO(crbug.com/881591): Remove this when ViewsV1 deprecation is complete.
  void OnEvent(fuchsia::ui::input::InputEvent event,
               OnEventCallback callback) override;

  // Callbacks for |scenic_session_|.
  void OnScenicError(zx_status_t status);
  void OnScenicEvents(fidl::VectorPtr<fuchsia::ui::scenic::Event> events);

  // InputEventDispatcher::Delegate interface.
  void DispatchEvent(ui::Event* event) override;

  // Error handler for |view_|. This error normally indicates the View was
  // destroyed (e.g. dropping ViewOwner).
  void OnViewError(zx_status_t status);

  void UpdateSize();

  ScenicWindowManager* const manager_;
  PlatformWindowDelegate* const delegate_;
  gfx::AcceleratedWidget const window_id_;

  // Dispatches Scenic input events as Chrome ui::Events.
  InputEventDispatcher event_dispatcher_;

  // Underlying View in the view_manager.
  fuchsia::ui::viewsv1::ViewPtr view_;
  fidl::Binding<fuchsia::ui::viewsv1::ViewListener> view_listener_binding_;

  // Scenic session used for all drawing operations in this View.
  scenic::Session scenic_session_;

  // Node in |scenic_session_| for the parent view.
  scenic::ImportNode parent_node_;

  // Node in |scenic_session_| for the parent view.
  scenic::EntityNode node_;

  // Node in |scenic_session_| for receiving input that hits within our View.
  scenic::ShapeNode input_node_;

  // Node in |scenic_session_| for rendering (hit testing disabled).
  scenic::EntityNode render_node_;

  // The ratio used for translating device-independent coordinates to absolute
  // pixel coordinates.
  float device_pixel_ratio_ = 0.f;

  // Current view size in DIPs.
  gfx::SizeF size_dips_;

  // Current view size in device pixels.
  gfx::Size size_pixels_;

  // InputConnection and InputListener binding used to receive input events from
  // the view.
  fuchsia::ui::input::InputConnectionPtr input_connection_;
  fidl::Binding<fuchsia::ui::input::InputListener> input_listener_binding_;

  DISALLOW_COPY_AND_ASSIGN(ScenicWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
