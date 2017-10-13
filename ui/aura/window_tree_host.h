// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_H_
#define UI_AURA_WINDOW_TREE_HOST_H_

#include <memory>
#include <vector>

#include "base/event_types.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/aura/aura_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Insets;
class Point;
class Rect;
class Size;
class Transform;
}

namespace ui {
class Compositor;
class EventSink;
class InputMethod;
class ViewProp;
}

namespace aura {
namespace test {
class WindowTreeHostTestApi;
}

class WindowEventDispatcher;
class WindowPort;
class WindowTreeHostObserver;

// WindowTreeHost bridges between a native window and the embedded RootWindow.
// It provides the accelerated widget and maps events from the native os to
// aura.
class AURA_EXPORT WindowTreeHost : public ui::internal::InputMethodDelegate,
                                   public ui::EventSource,
                                   public display::DisplayObserver,
                                   public ui::CompositorObserver {
 public:
  ~WindowTreeHost() override;

  // Creates a new WindowTreeHost. The caller owns the returned value.
  static WindowTreeHost* Create(const gfx::Rect& bounds_in_pixels);

  // Returns the WindowTreeHost for the specified accelerated widget, or NULL
  // if there is none associated.
  static WindowTreeHost* GetForAcceleratedWidget(gfx::AcceleratedWidget widget);

  void InitHost();

  void AddObserver(WindowTreeHostObserver* observer);
  void RemoveObserver(WindowTreeHostObserver* observer);

  Window* window() { return window_; }
  const Window* window() const { return window_; }

  ui::EventSink* event_sink();

  WindowEventDispatcher* dispatcher() {
    return const_cast<WindowEventDispatcher*>(
        const_cast<const WindowTreeHost*>(this)->dispatcher());
  }
  const WindowEventDispatcher* dispatcher() const { return dispatcher_.get(); }

  ui::Compositor* compositor() { return compositor_.get(); }

  // Gets/Sets the root window's transform.
  virtual gfx::Transform GetRootTransform() const;
  virtual void SetRootTransform(const gfx::Transform& transform);
  virtual gfx::Transform GetInverseRootTransform() const;

  // These functions are used in event translation for translating the local
  // coordinates of LocatedEvents. Default implementation calls to non-local
  // ones (e.g. GetRootTransform()).
  virtual gfx::Transform GetRootTransformForLocalEventCoordinates() const;
  virtual gfx::Transform GetInverseRootTransformForLocalEventCoordinates()
      const;

  // Sets padding applied to the output surface. The output surface is sized to
  // to the size of the host plus output surface padding. |window()| is offset
  // by |padding_in_pixels|, that is, |window|'s origin is set to
  // padding_in_pixels.left(), padding_in_pixels.top().
  // This does not impact the bounds as returned from GetBounds(), only the
  // output surface size and location of window(). Additionally window() is
  // sized to the size set by bounds (more specifically the size passed to
  // OnHostResizedInPixels()), but the location of window() is set to that of
  // |padding_in_pixels|.
  void SetOutputSurfacePaddingInPixels(const gfx::Insets& padding_in_pixels);

  // Updates the root window's size using |host_size_in_pixels|, current
  // transform and outsets.
  virtual void UpdateRootWindowSizeInPixels(
      const gfx::Size& host_size_in_pixels);

  // Converts |point| from the root window's coordinate system to native
  // screen's.
  void ConvertDIPToScreenInPixels(gfx::Point* point) const;

  // Converts |point| from native screen coordinate system to the root window's.
  void ConvertScreenInPixelsToDIP(gfx::Point* point) const;

  // Converts |point| from the root window's coordinate system to the
  // host window's.
  void ConvertDIPToPixels(gfx::Point* point) const;

  // Converts |point| from the host window's coordinate system to the
  // root window's.
  void ConvertPixelsToDIP(gfx::Point* point) const;

  // Cursor.
  // Sets the currently-displayed cursor. If the cursor was previously hidden
  // via ShowCursor(false), it will remain hidden until ShowCursor(true) is
  // called, at which point the cursor that was last set via SetCursor() will be
  // used.
  void SetCursor(gfx::NativeCursor cursor);

  // Invoked when the cursor's visibility has changed.
  void OnCursorVisibilityChanged(bool visible);

  // Moves the cursor to the specified location relative to the root window.
  void MoveCursorToLocationInDIP(const gfx::Point& location_in_dip);

  // Moves the cursor to the |location_in_pixels| given in host coordinates.
  void MoveCursorToLocationInPixels(const gfx::Point& location_in_pixels);

  gfx::NativeCursor last_cursor() const { return last_cursor_; }

  // Gets the InputMethod instance, if NULL, creates & owns it.
  ui::InputMethod* GetInputMethod();
  bool has_input_method() const { return input_method_ != nullptr; }

  // Sets a shared unowned InputMethod. This is used when there is a singleton
  // InputMethod shared between multiple WindowTreeHost instances.
  //
  // This is used for Ash only. There are 2 reasons:
  // 1) ChromeOS virtual keyboard needs to receive ShowImeIfNeeded notification
  // from InputMethod. Multiple InputMethod instances makes it hard to
  // register/unregister the observer for that notification.
  // 2) For Ozone, there is no native focus state for the root window and
  // WindowTreeHost. See DrmWindowHost::CanDispatchEvent, the key events always
  // goes to the primary WindowTreeHost. And after InputMethod processed the key
  // event and continue dispatching it, WindowTargeter::FindTargetForEvent may
  // re-dispatch it to a different WindowTreeHost. So the singleton InputMethod
  // can make sure the correct InputMethod instance processes the key event no
  // matter which WindowTreeHost is the target for event. Please refer to the
  // test: ExtendedDesktopTest.KeyEventsOnLockScreen.
  //
  // TODO(shuchen): remove this method after above reasons become invalid.
  // A possible solution is to make sure DrmWindowHost can find the correct
  // WindowTreeHost to dispatch events.
  void SetSharedInputMethod(ui::InputMethod* input_method);

  // Overridden from ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* event) final;

  // Returns the EventSource responsible for dispatching events to the window
  // tree.
  virtual ui::EventSource* GetEventSource() = 0;

  // Returns the accelerated widget.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Shows the WindowTreeHost.
  void Show();

  // Hides the WindowTreeHost.
  void Hide();

  // Gets/Sets the size of the WindowTreeHost (in pixels).
  virtual gfx::Rect GetBoundsInPixels() const = 0;
  virtual void SetBoundsInPixels(const gfx::Rect& bounds_in_pixels) = 0;

  // Sets the OS capture to the root window.
  virtual void SetCapture() = 0;

  // Releases OS capture of the root window.
  virtual void ReleaseCapture() = 0;

 protected:
  friend class TestScreen;  // TODO(beng): see if we can remove/consolidate.

  WindowTreeHost();
  explicit WindowTreeHost(std::unique_ptr<WindowPort> window_port);

  void DestroyCompositor();
  void DestroyDispatcher();

  // If frame_sink_id is not passed in, one will be grabbed from
  // ContextFactoryPrivate.
  void CreateCompositor(
      const viz::FrameSinkId& frame_sink_id = viz::FrameSinkId(),
      bool force_software_compositor = false,
      bool external_begin_frames_enabled = false);

  void InitCompositor();
  void OnAcceleratedWidgetAvailable();

  // Returns the location of the RootWindow on native screen.
  virtual gfx::Point GetLocationOnScreenInPixels() const = 0;

  void OnHostMovedInPixels(const gfx::Point& new_location_in_pixels);
  void OnHostResizedInPixels(const gfx::Size& new_size_in_pixels);
  void OnHostWorkspaceChanged();
  void OnHostDisplayChanged();
  void OnHostCloseRequested();
  void OnHostActivated();
  void OnHostLostWindowCapture();

  // Sets the currently displayed cursor.
  virtual void SetCursorNative(gfx::NativeCursor cursor) = 0;

  // Moves the cursor to the specified location relative to the root window.
  virtual void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) = 0;

  // Called when the cursor visibility has changed.
  virtual void OnCursorVisibilityChangedNative(bool show) = 0;

  // Shows the WindowTreeHost.
  virtual void ShowImpl() = 0;

  // Hides the WindowTreeHost.
  virtual void HideImpl() = 0;

  // Overridden from ui::EventSource:
  ui::EventSink* GetEventSink() override;

  // display::DisplayObserver implementation.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  friend class test::WindowTreeHostTestApi;

  // Moves the cursor to the specified location. This method is internally used
  // by MoveCursorToLocationInDIP() and MoveCursorToLocationInPixels().
  void MoveCursorToInternal(const gfx::Point& root_location,
                            const gfx::Point& host_location);

  // Overrided from CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override;
  void OnCompositingChildResizing(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // We don't use a std::unique_ptr for |window_| since we need this ptr to be
  // valid during its deletion. (Window's dtor notifies observers that may
  // attempt to reach back up to access this object which will be valid until
  // the end of the dtor).
  Window* window_;  // Owning.

  base::ObserverList<WindowTreeHostObserver> observers_;

  std::unique_ptr<WindowEventDispatcher> dispatcher_;

  std::unique_ptr<ui::Compositor> compositor_;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;
  gfx::Point last_cursor_request_position_in_host_;

  std::unique_ptr<ui::ViewProp> prop_;

  // The InputMethod instance used to process key events.
  // If owned it, it is created in GetInputMethod() method;
  // If not owned it, it is passed in through SetSharedInputMethod() method.
  ui::InputMethod* input_method_;

  // Whether the InputMethod instance is owned by this WindowTreeHost.
  bool owned_input_method_;

  gfx::Insets output_surface_padding_in_pixels_;

  // Set to true if the next CompositorFrame will block on a new child surface.
  bool synchronizing_with_child_on_next_frame_ = false;

  // Set to true if this WindowTreeHost is currently holding pointer moves.
  bool holding_pointer_moves_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHost);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_H_
