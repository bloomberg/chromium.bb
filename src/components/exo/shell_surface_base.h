// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_BASE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_BASE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_observer.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
class WindowState;
}  // namespace ash

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace exo {
class Surface;

// This class provides functions for treating a surfaces like toplevel,
// fullscreen or popup widgets, move, resize or maximize them, associate
// metadata like title and class, etc.
class ShellSurfaceBase : public SurfaceTreeHost,
                         public SurfaceObserver,
                         public aura::WindowObserver,
                         public aura::client::CaptureClientObserver,
                         public views::WidgetDelegate,
                         public views::WidgetObserver,
                         public views::View,
                         public wm::ActivationChangeObserver {
 public:
  // The |origin| is the initial position in screen coordinates. The position
  // specified as part of the geometry is relative to the shell surface.
  ShellSurfaceBase(Surface* surface,
                   const gfx::Point& origin,
                   bool activatable,
                   bool can_minimize,
                   int container);
  ~ShellSurfaceBase() override;

  // Set the callback to run when the user wants the shell surface to be closed.
  // The receiver can chose to not close the window on this signal.
  void set_close_callback(const base::RepeatingClosure& close_callback) {
    close_callback_ = close_callback;
  }

  // Set the callback to run when the user has requested to close the surface.
  // This runs before the normal |close_callback_| and should not be used to
  // actually close the surface.
  void set_pre_close_callback(const base::RepeatingClosure& close_callback) {
    pre_close_callback_ = close_callback;
  }

  // Set the callback to run when the surface is destroyed.
  void set_surface_destroyed_callback(
      base::OnceClosure surface_destroyed_callback) {
    surface_destroyed_callback_ = std::move(surface_destroyed_callback);
  }

  // Activates the shell surface.
  void Activate();

  // Set title for the surface.
  void SetTitle(const base::string16& title);

  // Set icon for the surface.
  void SetIcon(const gfx::ImageSkia& icon);

  // Sets the system modality.
  void SetSystemModal(bool system_modal);

  // Set the application ID for the surface.
  void SetApplicationId(const char* application_id);

  // Set the startup ID for the surface.
  void SetStartupId(const char* startup_id);

  // Set the child ax tree ID for the surface.
  void SetChildAxTreeId(ui::AXTreeID child_ax_tree_id);

  // Set geometry for surface. The geometry represents the "visible bounds"
  // for the surface from the user's perspective.
  void SetGeometry(const gfx::Rect& geometry);

  // If set, geometry is in display rather than window or screen coordinates.
  void SetDisplay(int64_t display_id);

  // Set origin in screen coordinate space.
  void SetOrigin(const gfx::Point& origin);

  // Set activatable state for surface.
  void SetActivatable(bool activatable);

  // Set container for surface.
  void SetContainer(int container);

  // Set the maximum size for the surface.
  void SetMaximumSize(const gfx::Size& size);

  // Set the miniumum size for the surface.
  void SetMinimumSize(const gfx::Size& size);

  // Set the aspect ratio for the surface.
  void SetAspectRatio(const gfx::SizeF& aspect_ratio);

  // Set the flag if the surface can maximize or not.
  void SetCanMinimize(bool can_minimize);

  // Prevents shell surface from being moved.
  void DisableMovement();

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override;
  void OnSetStartupId(const char* startup_id) override;
  void OnSetApplicationId(const char* application_id) override;
  void OnActivationRequested() override;

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override;

  // views::WidgetDelegate:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  bool frame_enabled() const {
    return frame_type_ != SurfaceFrameType::NONE &&
           frame_type_ != SurfaceFrameType::SHADOW;
  }

  Surface* surface_for_testing() { return root_surface(); }

 protected:
  // Creates the |widget_| for |surface_|. |show_state| is the initial state
  // of the widget (e.g. maximized).
  void CreateShellSurfaceWidget(ui::WindowShowState show_state);

  // Returns true if surface is currently being resized.
  bool IsResizing() const;

  // Updates the bounds of widget to match the current surface bounds.
  void UpdateWidgetBounds();

  // Called by UpdateWidgetBounds to set widget bounds.
  virtual void SetWidgetBounds(const gfx::Rect& bounds) = 0;

  // Updates the bounds of surface to match the current widget bounds.
  void UpdateSurfaceBounds();

  // Creates, deletes and update the shadow bounds based on
  // |shadow_bounds_|.
  void UpdateShadow();

  // Applies |system_modal_| to |widget_|.
  void UpdateSystemModal();

  // Returns the "visible bounds" for the surface from the user's perspective.
  gfx::Rect GetVisibleBounds() const;

  // Returns the bounds of the client area.nnn
  gfx::Rect GetClientViewBounds() const;

  // In the local coordinate system of the window.
  virtual gfx::Rect GetShadowBounds() const;

  // Start the event capture on this surface.
  void StartCapture();

  const gfx::Rect& geometry() const { return geometry_; }

  // Install custom window targeter. Used to restore window targeter.
  void InstallCustomWindowTargeter();

  // Creates a NonClientFrameView for shell surface.
  views::NonClientFrameView* CreateNonClientFrameViewInternal(
      views::Widget* widget,
      bool client_controlled);

  views::Widget* widget_ = nullptr;
  aura::Window* parent_ = nullptr;
  bool movement_disabled_ = false;
  gfx::Point origin_;

  // Container Window Id (see ash/public/cpp/shell_window_ids.h)
  int container_;
  gfx::Rect geometry_;
  gfx::Rect pending_geometry_;
  int64_t display_id_ = display::kInvalidDisplayId;
  int64_t pending_display_id_ = display::kInvalidDisplayId;
  base::Optional<gfx::Rect> shadow_bounds_;
  bool shadow_bounds_changed_ = false;
  base::string16 title_;
  SurfaceFrameType frame_type_ = SurfaceFrameType::NONE;
  bool is_popup_ = false;
  bool has_grab_ = false;

 private:
  FRIEND_TEST_ALL_PREFIXES(ShellSurfaceTest,
                           HostWindowBoundsUpdatedAfterCommitWidget);

  // Called on widget creation to initialize its window state.
  // TODO(reveman): Remove virtual functions below to avoid FBC problem.
  virtual void InitializeWindowState(ash::WindowState* window_state) = 0;

  // Returns the scale of the surface tree relative to the shell surface.
  virtual float GetScale() const;

  // Return the bounds of the widget/origin of surface taking visible
  // bounds and current resize direction into account.
  virtual base::Optional<gfx::Rect> GetWidgetBounds() const = 0;
  virtual gfx::Point GetSurfaceOrigin() const = 0;

  // Commit is deferred if this returns false.
  virtual bool OnPreWidgetCommit() = 0;
  virtual void OnPostWidgetCommit() = 0;

  void CommitWidget();

  bool activatable_ = true;
  bool can_minimize_ = true;
  bool has_frame_colors_ = false;
  SkColor active_frame_color_ = SK_ColorBLACK;
  SkColor inactive_frame_color_ = SK_ColorBLACK;
  bool pending_show_widget_ = false;
  base::Optional<std::string> application_id_;
  base::Optional<std::string> startup_id_;
  base::RepeatingClosure close_callback_;
  base::RepeatingClosure pre_close_callback_;
  base::OnceClosure surface_destroyed_callback_;
  bool system_modal_ = false;
  bool non_system_modal_window_was_active_ = false;
  gfx::ImageSkia icon_;
  gfx::Size minimum_size_;
  gfx::Size pending_minimum_size_;
  gfx::Size maximum_size_;
  gfx::Size pending_maximum_size_;
  gfx::SizeF pending_aspect_ratio_;
  ui::AXTreeID child_ax_tree_id_ = ui::AXTreeIDUnknown();

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceBase);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_BASE_H_
