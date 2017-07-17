// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EVENT_DISPATCHER_H_
#define SERVICES_UI_WS_EVENT_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/cursor/cursor.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/ws/drag_cursor_updater.h"
#include "services/ui/ws/event_matcher.h"
#include "services/ui/ws/event_targeter.h"
#include "services/ui/ws/event_targeter_delegate.h"
#include "services/ui/ws/modal_window_controller.h"
#include "services/ui/ws/server_window_observer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {
class Event;
class KeyEvent;
class LocatedEvent;
class PointerEvent;

namespace ws {

class Accelerator;
class DragController;
class DragSource;
class DragTargetConnection;
class EventDispatcherDelegate;
class ServerWindow;

namespace test {
class EventDispatcherTestApi;
}

// Handles dispatching events to the right location as well as updating focus.
class EventDispatcher : public ServerWindowObserver,
                        public DragCursorUpdater,
                        public EventTargeterDelegate {
 public:
  enum class AcceleratorMatchPhase {
    // Both pre and post should be considered.
    ANY,

    // PRE_TARGETs are not considered, only the actual target and any
    // accelerators registered with POST_TARGET.
    POST_ONLY,
  };

  explicit EventDispatcher(EventDispatcherDelegate* delegate);
  ~EventDispatcher() override;

  // Cancels capture and stops tracking any pointer events. This does not send
  // any events to the delegate.
  void Reset();

  void SetMousePointerDisplayLocation(const gfx::Point& display_location,
                                      int64_t display_id);
  const gfx::Point& mouse_pointer_last_location() const {
    return mouse_pointer_last_location_;
  }
  int64_t mouse_pointer_display_id() const { return mouse_pointer_display_id_; }

  // Returns the cursor for the current target, or POINTER if the mouse is not
  // over a valid target.
  ui::CursorData GetCurrentMouseCursor() const;

  // |capture_window_| will receive all input. See window_tree.mojom for
  // details.
  ServerWindow* capture_window() { return capture_window_; }
  const ServerWindow* capture_window() const { return capture_window_; }
  // Setting capture can fail if the window is blocked by a modal window
  // (indicated by returning |false|).
  bool SetCaptureWindow(ServerWindow* capture_window,
                        ClientSpecificId client_id);

  // Id of the client that capture events are sent to.
  ClientSpecificId capture_window_client_id() const {
    return capture_window_client_id_;
  }

  void SetDragDropSourceWindow(
      DragSource* drag_source,
      ServerWindow* window,
      DragTargetConnection* source_connection,
      int32_t drag_pointer,
      const std::unordered_map<std::string, std::vector<uint8_t>>& mime_data,
      uint32_t drag_operations);
  void CancelDragDrop();
  void EndDragDrop();

  void OnWillDestroyDragTargetConnection(DragTargetConnection* connection);

  // Adds a system modal window. The window remains modal to system until it is
  // destroyed. There can exist multiple system modal windows, in which case the
  // one that is visible and added most recently or shown most recently would be
  // the active one.
  void AddSystemModalWindow(ServerWindow* window);

  // Checks if |modal_window| is a visible modal window that blocks current
  // capture window and if that's the case, releases the capture.
  void ReleaseCaptureBlockedByModalWindow(const ServerWindow* modal_window);

  // Checks if the current capture window is blocked by any visible modal window
  // and if that's the case, releases the capture.
  void ReleaseCaptureBlockedByAnyModalWindow();

  // Retrieves the ServerWindow of the last mouse move. If there is no valid
  // window event target this falls back to the root of the display. In general
  // this is not null, but may be null during shutdown.
  ServerWindow* mouse_cursor_source_window() const {
    return mouse_cursor_source_window_;
  }

  // Returns the window the mouse cursor is taken from. This does not take
  // into account drags. In other words if there is a drag on going the mouse
  // comes comes from a different window.
  const ServerWindow* GetWindowForMouseCursor() const;

  // If the mouse cursor is still over |mouse_cursor_source_window_|, updates
  // whether we are in the non-client area. Used when
  // |mouse_cursor_source_window_| has changed its properties.
  void UpdateNonClientAreaForCurrentWindow();

  // Possibly updates the cursor. If we aren't in an implicit capture, we take
  // the last known location of the mouse pointer, and look for the
  // ServerWindow* under it.
  void UpdateCursorProviderByLastKnownLocation();

  // Adds an accelerator with the given id and event-matcher. If an accelerator
  // already exists with the same id or the same matcher, then the accelerator
  // is not added. Returns whether adding the accelerator was successful or not.
  bool AddAccelerator(uint32_t id, mojom::EventMatcherPtr event_matcher);

  void RemoveAccelerator(uint32_t id);

  void SetKeyEventsThatDontHideCursor(
      std::vector<::ui::mojom::EventMatcherPtr> dont_hide_cursor_list);

  // True if we are actively finding a target for an event, false otherwise.
  bool IsProcessingEvent() const;

  // Processes the supplied event, informing the delegate as approriate. This
  // may result in generating any number of events. If |match_phase| is
  // ANY and there is a matching accelerator with PRE_TARGET found, than only
  // OnAccelerator() is called. The expectation is after the PRE_TARGET has been
  // handled this is again called with an AcceleratorMatchPhase of POST_ONLY.
  // This may be asynchronous if we need to find the target window for |event|
  // asynchronously.
  void ProcessEvent(const ui::Event& event,
                    int64_t display_id,
                    AcceleratorMatchPhase match_phase);

  // EventTargeterDelegate:
  ServerWindow* GetRootWindowContaining(gfx::Point* location_in_display,
                                        int64_t* display_id) override;
  void ProcessNextAvailableEvent() override;

 private:
  friend class test::EventDispatcherTestApi;

  // Keeps track of state associated with an active pointer.
  struct PointerTarget {
    PointerTarget()
        : window(nullptr),
          is_mouse_event(false),
          in_nonclient_area(false),
          is_pointer_down(false) {}

    // The target window, which may be null. null is used in two situations:
    // when there is no valid window target, or there was a target but the
    // window is destroyed before a corresponding release/cancel.
    ServerWindow* window;

    bool is_mouse_event;

    // Did the pointer event start in the non-client area.
    bool in_nonclient_area;

    bool is_pointer_down;
  };

  void SetMouseCursorSourceWindow(ServerWindow* window);

  // Called after we found the target for the current mouse cursor to see if
  // |mouse_pointer_last_location_| and |mouse_pointer_display_id_| need to be
  // updated based on the new target we found. No need to call delegate's
  // OnMouseCursorLocationChanged since mouse location is the same in
  // screen-coord.
  // TODO(riajiang): No need to update mouse location after ozone drm can tell
  // us the right display the cursor is on for drag-n-drop events.
  // crbug.com/726470
  void SetMousePointerLocation(const gfx::Point& new_mouse_location,
                               int64_t new_mouse_display_id);

  void ProcessKeyEvent(const ui::KeyEvent& event,
                       AcceleratorMatchPhase match_phase);

  // When the user presses a key, we want to hide the cursor if it doesn't
  // match a list of window manager supplied keys.
  void HideCursorOnMatchedKeyEvent(const ui::KeyEvent& event);

  bool IsTrackingPointer(int32_t pointer_id) const {
    return pointer_targets_.count(pointer_id) > 0;
  }

  // EventDispatcher provides the following logic for pointer events:
  // . wheel events go to the current target of the associated pointer. If
  //   there is no target, they go to the deepest window.
  // . move (not drag) events go to the deepest window.
  // . when a pointer goes down all events until the corresponding up or
  //   cancel go to the deepest target. For mouse events the up only occurs
  //   when no buttons on the mouse are down.
  // This also generates exit events as appropriate. For example, if the mouse
  // moves between one window to another an exit is generated on the first.
  // |pointer_target| is the PointerTarget for |event| based on the
  // |deepest_window|, the deepest visible window for the root_location
  // of the |event|. |location_in_display| and |display_id| are updated values
  // for root_location and |event_display_id_| (e.g. during drag-n-drop).
  void ProcessPointerEventOnFoundTarget(const ui::PointerEvent& event,
                                        const LocationTarget& location_target);

  void UpdateNonClientAreaForCurrentWindowOnFoundWindow(
      const LocationTarget& location_target);
  void UpdateCursorProviderByLastKnownLocationOnFoundWindow(
      const LocationTarget& location_target);

  // Adds |pointer_target| to |pointer_targets_|.
  void StartTrackingPointer(int32_t pointer_id,
                            const PointerTarget& pointer_target);

  // Removes a PointerTarget from |pointer_targets_|.
  void StopTrackingPointer(int32_t pointer_id);

  // Starts tracking the pointer for |event|, or if already tracking the
  // pointer sends the appropriate event to the delegate and updates the
  // currently tracked PointerTarget appropriately.
  void UpdateTargetForPointer(int32_t pointer_id,
                              const ui::PointerEvent& event,
                              const PointerTarget& pointer_target);

  // Returns true if any pointers are in the pressed/down state.
  bool AreAnyPointersDown() const;

  // If |target->window| is valid, then passes the event to the delegate.
  void DispatchToPointerTarget(const PointerTarget& target,
                               const ui::LocatedEvent& event);

  // Dispatch |event| to the delegate.
  void DispatchToClient(ServerWindow* window,
                        ClientSpecificId client_id,
                        const ui::LocatedEvent& event);

  // Stops sending pointer events to |window|. This does not remove the entry
  // for |window| from |pointer_targets_|, rather it nulls out the window. This
  // way we continue to eat events until the up/cancel is received.
  void CancelPointerEventsToTarget(ServerWindow* window);

  // Used to observe a window. Can be called multiple times on a window. To
  // unobserve a window, UnobserveWindow() should be called the same number of
  // times.
  void ObserveWindow(ServerWindow* winodw);
  void UnobserveWindow(ServerWindow* winodw);

  // Returns an Accelerator bound to the specified code/flags, and of the
  // matching |phase|. Otherwise returns null.
  Accelerator* FindAccelerator(const ui::KeyEvent& event,
                               const ui::mojom::AcceleratorPhase phase);

  // Clears the implicit captures in |pointer_targets_|, with the exception of
  // |window|. |window| may be null. |client_id| is the target client of
  // |window|.
  void CancelImplicitCaptureExcept(ServerWindow* window,
                                   ClientSpecificId client_id);

  // ServerWindowObserver:
  void OnWillChangeWindowHierarchy(ServerWindow* window,
                                   ServerWindow* new_parent,
                                   ServerWindow* old_parent) override;
  void OnWindowVisibilityChanged(ServerWindow* window) override;
  void OnWindowDestroyed(ServerWindow* window) override;

  // DragCursorUpdater:
  void OnDragCursorUpdated() override;

  EventDispatcherDelegate* delegate_;

  ServerWindow* capture_window_;
  ClientSpecificId capture_window_client_id_;

  std::unique_ptr<DragController> drag_controller_;

  ModalWindowController modal_window_controller_;

  std::unique_ptr<EventTargeter> event_targeter_;

  bool mouse_button_down_;
  ServerWindow* mouse_cursor_source_window_;
  bool mouse_cursor_in_non_client_area_;

  // The location of the mouse pointer in display coordinates. This can be
  // outside the bounds of |mouse_cursor_source_window_|, which can capture the
  // cursor.
  gfx::Point mouse_pointer_last_location_;
  // Id of the display |mouse_pointer_last_location_| is on.
  int64_t mouse_pointer_display_id_ = display::kInvalidDisplayId;

  // Id of the display the most recent event is on.
  int64_t event_display_id_ = display::kInvalidDisplayId;

  std::map<uint32_t, std::unique_ptr<Accelerator>> accelerators_;

  // A list of EventMatchers provided by the window manager. When this list is
  // empty, we perform no processing. When it contains EventMatchers, we run
  // each post-targeted key event through this list and if there's a match, we
  // don't hide the cursor (otherwise we hide the cursor).
  std::vector<EventMatcher> dont_hide_cursor_matchers_;

  using PointerIdToTargetMap = std::map<int32_t, PointerTarget>;
  // |pointer_targets_| contains the active pointers. For a mouse based pointer
  // a PointerTarget is always active (and present in |pointer_targets_|). For
  // touch based pointers the pointer is active while down and removed on
  // cancel or up.
  PointerIdToTargetMap pointer_targets_;

  // Keeps track of number of observe requests for each observed window.
  std::map<const ServerWindow*, uint8_t> observed_windows_;

#if !defined(NDEBUG)
  std::unique_ptr<ui::Event> previous_event_;
  AcceleratorMatchPhase previous_accelerator_match_phase_ =
      AcceleratorMatchPhase::ANY;
#endif

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EVENT_DISPATCHER_H_
