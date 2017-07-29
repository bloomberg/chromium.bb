// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_MANAGER_STATE_H_
#define SERVICES_UI_WS_WINDOW_MANAGER_STATE_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "services/ui/public/interfaces/display_manager.mojom.h"
#include "services/ui/ws/cursor_state.h"
#include "services/ui/ws/cursor_state_delegate.h"
#include "services/ui/ws/event_dispatcher.h"
#include "services/ui/ws/event_dispatcher_delegate.h"
#include "services/ui/ws/server_window_observer.h"
#include "services/ui/ws/user_id.h"
#include "services/ui/ws/window_server.h"

namespace ui {
namespace ws {

class DisplayManager;
class PlatformDisplay;
class WindowManagerDisplayRoot;
class WindowTree;

namespace test {
class WindowManagerStateTestApi;
}

// Manages state specific to a WindowManager that is shared across displays.
// WindowManagerState is owned by the WindowTree the window manager is
// associated with.
class WindowManagerState : public EventDispatcherDelegate,
                           public ServerWindowObserver,
                           public CursorStateDelegate {
 public:
  explicit WindowManagerState(WindowTree* window_tree);
  ~WindowManagerState() override;

  const UserId& user_id() const;

  WindowTree* window_tree() { return window_tree_; }
  const WindowTree* window_tree() const { return window_tree_; }

  void OnWillDestroyTree(WindowTree* tree);

  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }
  bool got_frame_decoration_values() const {
    return got_frame_decoration_values_;
  }

  bool SetCapture(ServerWindow* window, ClientSpecificId client_id);
  ServerWindow* capture_window() { return event_dispatcher_.capture_window(); }
  const ServerWindow* capture_window() const {
    return event_dispatcher_.capture_window();
  }

  void ReleaseCaptureBlockedByModalWindow(const ServerWindow* modal_window);
  void ReleaseCaptureBlockedByAnyModalWindow();

  // Sets the location of the cursor to a location on display |display_id|.
  void SetCursorLocation(const gfx::Point& display_pixels, int64_t display_id);

  void SetKeyEventsThatDontHideCursor(
      std::vector<::ui::mojom::EventMatcherPtr> dont_hide_cursor_list);

  void SetCursorTouchVisible(bool enabled);

  void SetDragDropSourceWindow(
      DragSource* drag_source,
      ServerWindow* window,
      DragTargetConnection* source_connection,
      const std::unordered_map<std::string, std::vector<uint8_t>>& drag_data,
      uint32_t drag_operation);
  void CancelDragDrop();
  void EndDragDrop();

  void AddSystemModalWindow(ServerWindow* window);

  // Deletes the WindowManagerDisplayRoot whose root is |display_root|.
  void DeleteWindowManagerDisplayRoot(ServerWindow* display_root);

  // Returns the ServerWindow corresponding to an orphaned root with the
  // specified id. See |orphaned_window_manager_display_roots_| for details on
  // what on orphaned root is.
  ServerWindow* GetOrphanedRootWithId(const WindowId& id);

  // TODO(sky): EventDispatcher is really an implementation detail and should
  // not be exposed.
  EventDispatcher* event_dispatcher() { return &event_dispatcher_; }

  CursorState& cursor_state() { return cursor_state_; }

  // Returns true if this is the WindowManager of the active user.
  bool IsActive() const;

  void Activate(const gfx::Point& mouse_location_on_display,
                int64_t display_id);
  void Deactivate();

  // Processes an event from PlatformDisplay.
  void ProcessEvent(const Event& event, int64_t display_id);

 private:
  class ProcessedEventTarget;
  friend class Display;
  friend class test::WindowManagerStateTestApi;

  // Set of display roots. This is a vector rather than a set to support removal
  // without deleting.
  using WindowManagerDisplayRoots =
      std::vector<std::unique_ptr<WindowManagerDisplayRoot>>;

  enum class DebugAcceleratorType {
    PRINT_WINDOWS,
  };

  struct DebugAccelerator {
    bool Matches(const KeyEvent& event) const;

    DebugAcceleratorType type;
    KeyboardCode key_code;
    int event_flags;
  };

  enum class EventDispatchPhase {
    // Not actively dispatching.
    NONE,

    // A PRE_TARGET accelerator has been encountered and we're awaiting the ack.
    PRE_TARGET_ACCELERATOR,

    // Dispatching to the target, awaiting the ack.
    TARGET,
  };

  // There are two types of events that may be queued, both occur only when
  // waiting for an ack from a client.
  // . We get an event from the PlatformDisplay. This results in |event| being
  //   set, but |processed_target| is null.
  // . We get an event from the EventDispatcher. In this case both |event| and
  //   |processed_target| are valid.
  // The second case happens if EventDispatcher generates more than one event
  // at a time.
  struct QueuedEvent {
    QueuedEvent();
    ~QueuedEvent();

    std::unique_ptr<Event> event;
    std::unique_ptr<ProcessedEventTarget> processed_target;
    int64_t display_id;
  };

  // Tracks state associated with an event being dispatched to a client.
  struct InFlightEventDispatchDetails {
    InFlightEventDispatchDetails(WindowManagerState* window_manager_state,
                                 WindowTree* tree,
                                 int64_t display_id,
                                 const Event& event,
                                 EventDispatchPhase phase);
    ~InFlightEventDispatchDetails();

    base::OneShotTimer timer;
    WindowTree* tree;
    int64_t display_id;
    std::unique_ptr<Event> event;
    EventDispatchPhase phase;
    base::WeakPtr<Accelerator> post_target_accelerator;
    // Used for callbacks/timer specific to processing |event|.
    base::WeakPtrFactory<WindowManagerState> weak_factory;
  };

  const WindowServer* window_server() const;
  WindowServer* window_server();

  DisplayManager* display_manager();
  const DisplayManager* display_manager() const;

  // Adds |display_root| to the set of WindowManagerDisplayRoots owned by this
  // WindowManagerState.
  void AddWindowManagerDisplayRoot(
      std::unique_ptr<WindowManagerDisplayRoot> display_root);

  // Called when a Display is deleted.
  void OnDisplayDestroying(Display* display);

  // Sets the visibility of all window manager roots windows to |value|.
  void SetAllRootWindowsVisible(bool value);

  // Returns the ServerWindow that is the root of the WindowManager for
  // |window|. |window| corresponds to the root of a Display.
  ServerWindow* GetWindowManagerRootForDisplayRoot(ServerWindow* window);

  // Called from the callback supplied to WindowTree::OnAccelerator().
  void OnAcceleratorAck(
      mojom::EventResult result,
      const std::unordered_map<std::string, std::vector<uint8_t>>& properties);

  // Called from the callback supplied to WindowTree::DispatchInputEvent().
  void OnEventAck(mojom::WindowTree* tree, mojom::EventResult result);

  // Called if the client doesn't ack an event in the appropriate amount of
  // time.
  void OnEventAckTimeout(ClientSpecificId client_id);

  // Implemenation of processing an event with a match phase of all. This
  // handles debug accelerators and forwards to EventDispatcher.
  void ProcessEventImpl(const Event& event, int64_t display_id);

  // Schedules an event to be processed later.
  void QueueEvent(const Event& event,
                  std::unique_ptr<ProcessedEventTarget> processed_event_target,
                  int64_t display_id);

  // Dispatches the event to the appropriate client and starts the ack timer.
  void DispatchInputEventToWindowImpl(ServerWindow* target,
                                      ClientSpecificId client_id,
                                      int64_t display_id,
                                      const Event& event,
                                      base::WeakPtr<Accelerator> accelerator);

  // Registers accelerators used internally for debugging.
  void AddDebugAccelerators();

  // Finds the debug accelerator for |event| and if one is found calls
  // HandleDebugAccelerator().
  void ProcessDebugAccelerator(const Event& event, int64_t display_id);

  // Runs the specified debug accelerator.
  void HandleDebugAccelerator(DebugAcceleratorType type, int64_t display_id);

  // Called when waiting for an event or accelerator to be processed by |tree|.
  void ScheduleInputEventTimeout(WindowTree* tree,
                                 ServerWindow* target,
                                 int64_t display_id,
                                 const Event& event,
                                 EventDispatchPhase phase);

  // Helper function to convert |point| to be in screen coordinates. |point| as
  // the input should be in display-physical-pixel space, and the output is in
  // screen-dip space. Returns true if the |point| is successfully converted,
  // false otherwise.
  bool ConvertPointToScreen(int64_t display_id, gfx::Point* point);

  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator_id,
                     int64_t display_id,
                     const Event& event,
                     AcceleratorPhase phase) override;
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override;
  ServerWindow* GetFocusedWindowForEventDispatcher(int64_t display_id) override;
  void SetNativeCapture(ServerWindow* window) override;
  void ReleaseNativeCapture() override;
  void UpdateNativeCursorFromDispatcher() override;
  void OnCaptureChanged(ServerWindow* new_capture,
                        ServerWindow* old_capture) override;
  void OnMouseCursorLocationChanged(const gfx::Point& point,
                                    int64_t display_id) override;
  void OnEventChangesCursorVisibility(const ui::Event& event,
                                      bool visible) override;
  void OnEventChangesCursorTouchVisibility(const ui::Event& event,
                                           bool visible) override;
  void DispatchInputEventToWindow(ServerWindow* target,
                                  ClientSpecificId client_id,
                                  int64_t display_id,
                                  const Event& event,
                                  Accelerator* accelerator) override;
  // Processes the next valid event in |event_queue_|. If the event has already
  // been processed it is dispatched, otherwise the event is passed to the
  // EventDispatcher for processing.
  void ProcessNextAvailableEvent() override;
  ClientSpecificId GetEventTargetClientId(const ServerWindow* window,
                                          bool in_nonclient_area) override;
  ServerWindow* GetRootWindowContaining(gfx::Point* location_in_display,
                                        int64_t* display_id) override;
  void OnEventTargetNotFound(const Event& event, int64_t display_id) override;

  // ServerWindowObserver:
  void OnWindowEmbeddedAppDisconnected(ServerWindow* window) override;

  // CursorStateDelegate:
  void OnCursorTouchVisibleChanged(bool enabled) override;

  // The single WindowTree this WindowManagerState is associated with.
  // |window_tree_| owns this.
  WindowTree* window_tree_;

  // Set to true the first time SetFrameDecorationValues() is called.
  bool got_frame_decoration_values_ = false;
  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  // Events can go into this queue if there's a hit-test in flight in
  // EventDispatcher or if we are actively dispatching an event.
  std::queue<std::unique_ptr<QueuedEvent>> event_queue_;

  std::vector<DebugAccelerator> debug_accelerators_;

  // If non-null we're actively waiting for a response from a client for an
  // event.
  std::unique_ptr<InFlightEventDispatchDetails>
      in_flight_event_dispatch_details_;

  EventDispatcher event_dispatcher_;

  // PlatformDisplay that currently has capture.
  PlatformDisplay* platform_display_with_capture_ = nullptr;

  // All the active WindowManagerDisplayRoots.
  WindowManagerDisplayRoots window_manager_display_roots_;

  // Set of WindowManagerDisplayRoots corresponding to Displays that have been
  // destroyed. WindowManagerDisplayRoots are not destroyed immediately when
  // the Display is destroyed to allow the client to destroy the window when it
  // wants to. Once the client destroys the window WindowManagerDisplayRoots is
  // destroyed.
  WindowManagerDisplayRoots orphaned_window_manager_display_roots_;

  // All state regarding what the current cursor is.
  CursorState cursor_state_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerState);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_MANAGER_STATE_H_
