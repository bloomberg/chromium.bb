// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_SERVICE_DELEGATE_H_
#define SERVICES_WS_WINDOW_SERVICE_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class PropertyConverter;
class Window;
}  // namespace aura

namespace gfx {
class Point;
}

namespace mojo {
class ScopedInterfaceEndpointHandle;
}

namespace ui {
class EventTarget;
class KeyEvent;
class OSExchangeData;
class SystemInputInjector;
}  // namespace ui

namespace ws {

class TopLevelProxyWindow;
class WindowManagerInterface;
class WindowTree;

// A delegate used by the WindowService for context-specific operations.
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowServiceDelegate {
 public:
  // A client requested a new top-level window. Implementations should create a
  // new window, parenting it in the appropriate container. Return null to
  // reject the request.
  // |top_level_proxy_window| is owned by the WindowService and may be used by
  // the delegate to perform operations specific to the window. See
  // TopLevelProxyWindow for details.
  // NOTE: it is recommended that when clients create a new window they use
  // WindowDelegateImpl as the WindowDelegate of the Window (this must be done
  // by the WindowServiceDelegate, as the Window's delegate can not be changed
  // after creation).
  virtual std::unique_ptr<aura::Window> NewTopLevel(
      TopLevelProxyWindow* top_level_proxy_window,
      aura::PropertyConverter* property_converter,
      const base::flat_map<std::string, std::vector<uint8_t>>& properties) = 0;

  // Called for KeyEvents the client does not handle.
  virtual void OnUnhandledKeyEvent(const ui::KeyEvent& key_event) {}

  // Sets the cursor for |window| to |cursor|. This will immediately change the
  // actual on-screen cursor if the pointer is hovered over |window|. Also store
  // |cursor| on the widget for |window| if there is one. The return value
  // indicates whether the cursor was stored for |window|.
  virtual bool StoreAndSetCursor(aura::Window* window, ui::Cursor cursor);

  // Called to start a move operation on |window|. When done, |callback| should
  // be run with the result (true if the move was successful). If a move is not
  // allowed, the delegate should run |callback| immediately.
  using DoneCallback = base::OnceCallback<void(bool)>;
  virtual void RunWindowMoveLoop(aura::Window* window,
                                 mojom::MoveLoopSource source,
                                 const gfx::Point& cursor,
                                 int window_component,
                                 DoneCallback callback);

  // Called to cancel an in-progress window move loop that was started by
  // RunWindowMoveLoop().
  virtual void CancelWindowMoveLoop() {}

  // Called to run a drag loop for |window|. When done, |callback| should be
  // invoked with the |drag_result|. |drag_result| == DRAG_NONE means drag
  // failed or is canceled. Otherwise, it the final drag operation applied at
  // the end. If a drag is not allowed, the delegate should run |callback|
  // immediately. Note this call blocks until the drag operation is finished or
  // canceled.
  using DragDropCompletedCallback = base::OnceCallback<void(int drag_result)>;
  virtual void RunDragLoop(aura::Window* window,
                           const ui::OSExchangeData& data,
                           const gfx::Point& screen_location,
                           uint32_t drag_operation,
                           ui::DragDropTypes::DragEventSource source,
                           DragDropCompletedCallback callback);

  // Called to cancel an in-progress drag loop that was started by RunDragLoop.
  virtual void CancelDragLoop(aura::Window* window) {}

  // Called to update resize shadow for the window.
  virtual void SetWindowResizeShadow(aura::Window* window,
                                     int window_component) {}

  // Called to update the text input state of the PlatformWindow associated with
  // |window|. It is a no-op if |window| is not focused.
  virtual void UpdateTextInputState(aura::Window* window,
                                    ui::mojom::TextInputStatePtr state) {}

  // Called to update the IME visibility and text input state of the
  // PlatformWindow associated with |window|. It is a no-op if |window| is not
  // focused.
  virtual void UpdateImeVisibility(aura::Window* window,
                                   bool visible,
                                   ui::mojom::TextInputStatePtr state) {}

  // Called to set the window's modal type; may reparent the window.
  virtual void SetModalType(aura::Window* window, ui::ModalType type) {}

  // Returns the SystemInputInjector to use when processing events from a
  // remote client. A return value of null (the default) results in disallowing
  // injection.
  virtual ui::SystemInputInjector* GetSystemInputInjector();

  // Returns the EventTarget which can process all of the events on the system.
  virtual ui::EventTarget* GetGlobalEventTarget() = 0;

  virtual aura::Window* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Returns the topmost visible window at the location in screen coordinate,
  // excluding |ignore|. |real_topmost| is updated to the topmost visible window
  // at the location without excluding |ignore|.
  virtual aura::Window* GetTopmostWindowAtPoint(
      const gfx::Point& location_in_screen,
      const std::set<aura::Window*>& ignore,
      aura::Window** real_topmost);

  // Creates and binds a request for an interface provided by the local
  // environment. The interface request originated from the client associated
  // with |tree|. |name| is the name of the requested interface. The return
  // value is owned by |tree|. Return null if |name| is not the name of a known
  // interface.
  // The following shows how to bind |handle|:
  // TestWmInterface* wm_interface_impl = ...;
  // mojo::AssociatedBindingTestWmInterface> binding(
  //   wm_interface_impl,
  //   mojo::AssociatedInterfaceRequest<TestWmInterface>(std::move(handle)));
  virtual std::unique_ptr<WindowManagerInterface> CreateWindowManagerInterface(
      WindowTree* tree,
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle);

 protected:
  virtual ~WindowServiceDelegate() = default;
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_SERVICE_DELEGATE_H_
