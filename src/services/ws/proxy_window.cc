// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/proxy_window.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ws/client_root.h"
#include "services/ws/drag_drop_delegate.h"
#include "services/ws/embedding.h"
#include "services/ws/top_level_proxy_window_impl.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_utils.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event_handler.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/window_modality_controller.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ws::ProxyWindow*)

namespace ws {
namespace {
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ProxyWindow, kProxyWindowKey, nullptr)

bool IsPointerPressedEvent(const ui::Event& event) {
  return event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_TOUCH_PRESSED;
}

bool IsPointerEvent(const ui::Event& event) {
  return event.IsMouseEvent() || event.IsTouchEvent();
}

bool IsLastMouseButtonRelease(const ui::Event& event) {
  return event.type() == ui::ET_MOUSE_RELEASED &&
         event.AsMouseEvent()->button_flags() ==
             event.AsMouseEvent()->changed_button_flags();
}

bool IsPointerReleased(const ui::Event& event) {
  return IsLastMouseButtonRelease(event) ||
         event.type() == ui::ET_TOUCH_RELEASED;
}

ui::PointerId GetPointerId(const ui::Event& event) {
  if (event.IsMouseEvent())
    return ui::MouseEvent::kMousePointerId;
  DCHECK(event.IsTouchEvent());
  return event.AsTouchEvent()->pointer_details().id;
}

// ProxyWindowEventHandler is used to forward events to the client.
// ProxyWindowEventHandler adds itself to the pre-phase to ensure it's
// considered before the Window's delegate (or other EventHandlers).
class ProxyWindowEventHandler : public ui::EventHandler {
 public:
  explicit ProxyWindowEventHandler(ProxyWindow* proxy_window)
      : proxy_window_(proxy_window) {
    // Use |kDefault| so as not to conflict with other important pre-target
    // handlers (such as laser pointer).
    window()->AddPreTargetHandler(this, ui::EventTarget::Priority::kDefault);
  }
  ~ProxyWindowEventHandler() override {
    window()->RemovePreTargetHandler(this);
  }

  ProxyWindow* proxy_window() { return proxy_window_; }
  aura::Window* window() { return proxy_window_->window(); }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (event->phase() != ui::EP_PRETARGET) {
      // All work is done in the pre-phase. If this branch is hit, it means
      // event propagation was not stopped, and normal processing should
      // continue. Early out to avoid sending the event to the client again.
      return;
    }

    if (HandleInterceptedEvent(event) || ShouldIgnoreEvent(*event))
      return;

    auto* owning = proxy_window_->owning_window_tree();
    auto* embedded = proxy_window_->embedded_window_tree();
    WindowTree* target_client = nullptr;
    if (proxy_window_->DoesOwnerInterceptEvents()) {
      // A client that intercepts events, always gets the event regardless of
      // focus/capture.
      target_client = owning;
    } else if (event->IsKeyEvent()) {
      if (!proxy_window_->focus_owner())
        return;  // The local environment is going to process the event.
      target_client = proxy_window_->focus_owner();
    } else if (proxy_window()->capture_owner()) {
      target_client = proxy_window()->capture_owner();
    } else {
      // Prefer embedded over owner.
      target_client = !embedded ? owning : embedded;
    }
    DCHECK(target_client);

    // Don't send located events to the client during a move loop. Normally
    // the client shouldn't be the target at this point, but it's entirely
    // possible for held events to be released, triggering a spurious call.
    if (event->IsLocatedEvent() && target_client->IsMovingWindow())
      return;

    target_client->SendEventToClient(window(), *event);

    // The event was forwarded to the remote client. We don't want it handled
    // locally too.
    if (event->cancelable())
      event->StopPropagation();
  }

 protected:
  // Returns true if the event is a pinch event generated from the touchpad.
  bool IsPinchEventOnTouchpad(const ui::Event& event) {
    return event.IsPinchEvent() &&
           event.AsGestureEvent()->details().device_type() ==
               ui::GestureDeviceType::DEVICE_TOUCHPAD;
  }

  // Returns true if the event should be ignored (not forwarded to the client).
  bool ShouldIgnoreEvent(const ui::Event& event) {
    // It's assumed clients do their own gesture recognizition, which means
    // GestureEvents should not be forwarded to clients. Pinch events are
    // exceptional since they aren't created through gesture recognition but
    // from the touchpad directly. See https://crbug.com/933985.
    if (event.IsGestureEvent() && !IsPinchEventOnTouchpad(event))
      return true;

    if (static_cast<aura::Window*>(event.target()) != window()) {
      // As ProxyWindow is a EP_PRETARGET EventHandler it gets events *before*
      // descendants. Ignore all such events, and only process when
      // window() is the the target.
      return true;
    }
    if (wm::GetModalTransient(window()))
      return true;  // Do not send events to clients blocked by a modal window.
    return ShouldIgnoreEventType(event.type());
  }

  bool ShouldIgnoreEventType(ui::EventType type) const {
    // WindowTreeClient takes care of sending ET_MOUSE_CAPTURE_CHANGED at the
    // right point. The enter events are effectively synthetic, and indirectly
    // generated in the client as the result of a move event.
    switch (type) {
      case ui::ET_MOUSE_CAPTURE_CHANGED:
      case ui::ET_MOUSE_ENTERED:
        return true;
      default:
        break;
    }
    return false;
  }

  // If |window| identifies an embedding and the owning client intercepts
  // events, this forwards to the owner and returns true. Otherwise returns
  // false.
  bool HandleInterceptedEvent(ui::Event* event) {
    if (ShouldIgnoreEventType(event->type()))
      return false;

    // KeyEvents, and events when there is capture, do not go through through
    // ProxyWindowTargeter. As a result ProxyWindowEventHandler has to check
    // for a client intercepting events.
    if (proxy_window_->DoesOwnerInterceptEvents()) {
      proxy_window_->owning_window_tree()->SendEventToClient(window(), *event);
      if (event->cancelable())
        event->StopPropagation();
      return true;
    }
    return false;
  }

 private:
  ProxyWindow* const proxy_window_;

  DISALLOW_COPY_AND_ASSIGN(ProxyWindowEventHandler);
};

class TopLevelEventHandler;

// PointerPressHandler is used to track state while a pointer is down.
// PointerPressHandler is typically destroyed when the pointer is released, but
// it may be destroyed at other times, such as when capture changes.
class PointerPressHandler : public aura::client::CaptureClientObserver,
                            public aura::WindowObserver {
 public:
  PointerPressHandler(TopLevelEventHandler* top_level_event_handler,
                      ui::PointerId pointer_id,
                      const gfx::Point& location);
  ~PointerPressHandler() override;

  bool in_non_client_area() const { return in_non_client_area_; }

 private:
  // aura::client::CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  TopLevelEventHandler* top_level_event_handler_;

  // True if the pointer down occurred in the non-client area.
  const bool in_non_client_area_;

  // Id of the pointer the handler was created for.
  const ui::PointerId pointer_id_;

  DISALLOW_COPY_AND_ASSIGN(PointerPressHandler);
};

// ui::EventHandler used for top-levels. Some events that target the non-client
// area are not sent to the client, instead are handled locally. For example,
// if a press occurs in the non-client area, then the event is not sent to
// the client, it's handled locally.
class TopLevelEventHandler : public ProxyWindowEventHandler {
 public:
  explicit TopLevelEventHandler(ProxyWindow* proxy_window)
      : ProxyWindowEventHandler(proxy_window) {
    // Top-levels should always have an owning_window_tree().
    // OnEvent() assumes this.
    DCHECK(proxy_window->owning_window_tree());
  }

  ~TopLevelEventHandler() override = default;

  void DestroyPointerPressHandler(ui::PointerId id) {
    pointer_press_handlers_.erase(id);
  }

  // Returns true if the pointer with |pointer_id| was pressed over the
  // top-level. If this returns true, TopLevelEventHandler is waiting on a
  // release to reset state.
  bool IsHandlingPointerPress(ui::PointerId pointer_id) const {
    return pointer_press_handlers_.count(pointer_id) > 0;
  }

  // Called when the capture owner changes.
  void OnCaptureOwnerChanged() {
    // Changing the capture owner toggles between local and the client getting
    // the event. The |pointer_press_handlers_| are no longer applicable
    // (because the target is purely dicatated by capture owner).
    pointer_press_handlers_.clear();
  }

  // ProxyWindowEventHandler:
  void OnEvent(ui::Event* event) override {
    if (event->phase() != ui::EP_PRETARGET) {
      // All work is done in the pre-phase. If this branch is hit, it means
      // event propagation was not stopped, and normal processing should
      // continue. Early out to avoid sending the event to the client again.
      return;
    }

    if (HandleInterceptedEvent(event))
      return;

    if (!event->IsLocatedEvent()) {
      ProxyWindowEventHandler::OnEvent(event);
      return;
    }

    // When the gesture-end happens in the server side, the gesture state
    // is cleaned up there; this state should be synchronized with the client.
    if (event->type() == ui::ET_GESTURE_END &&
        event->AsGestureEvent()->details().touch_points() == 1) {
      proxy_window()->owning_window_tree()->CleanupGestureState(window());
      return;
    }

    if (ShouldIgnoreEvent(*event))
      return;

    // If there is capture, send the event to the client that owns it. A null
    // capture owner means the local environment should handle the event.
    if (wm::CaptureController::Get()->GetCaptureWindow()) {
      if (proxy_window()->capture_owner()) {
        proxy_window()->capture_owner()->SendEventToClient(window(), *event);
        if (event->cancelable())
          event->StopPropagation();
        return;
      }
      return;
    }

    // This code has two specific behaviors. It's used to ensure events go to
    // the right target (either local, or the remote client).
    // . a press-release sequence targets only one. If in non-client area then
    //   local, otherwise remote client.
    // . mouse-moves (not drags) go to both targets.
    bool stop_propagation = false;
    if (proxy_window()->HasNonClientArea() && IsPointerEvent(*event)) {
      const ui::PointerId pointer_id = GetPointerId(*event);
      if (!pointer_press_handlers_.count(pointer_id)) {
        if (IsPointerPressedEvent(*event)) {
          std::unique_ptr<PointerPressHandler> handler_ptr =
              std::make_unique<PointerPressHandler>(
                  this, pointer_id, event->AsLocatedEvent()->location());
          PointerPressHandler* handler = handler_ptr.get();
          pointer_press_handlers_[pointer_id] = std::move(handler_ptr);
          if (handler->in_non_client_area())
            return;  // Don't send presses in non-client area to client.
          stop_propagation = true;
        }
      } else {
        // Currently handling a pointer press and waiting on release.
        PointerPressHandler* handler =
            pointer_press_handlers_[pointer_id].get();
        const bool was_press_in_non_client_area = handler->in_non_client_area();
        if (IsPointerReleased(*event))
          pointer_press_handlers_.erase(pointer_id);
        if (was_press_in_non_client_area)
          return;  // Don't send release to client since press didn't go there.
        stop_propagation = true;
      }
    }
    proxy_window()->owning_window_tree()->SendEventToClient(window(), *event);
    if (stop_propagation && event->cancelable())
      event->StopPropagation();
  }

 private:
  // Non-null while in a pointer press press-drag-release cycle. Maps from
  // pointer-id of the pointer that is down to the handler.
  base::flat_map<ui::PointerId, std::unique_ptr<PointerPressHandler>>
      pointer_press_handlers_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelEventHandler);
};

PointerPressHandler::PointerPressHandler(
    TopLevelEventHandler* top_level_event_handler,
    ui::PointerId pointer_id,
    const gfx::Point& location)
    : top_level_event_handler_(top_level_event_handler),
      in_non_client_area_(
          IsLocationInNonClientArea(top_level_event_handler->window(),
                                    location)),
      pointer_id_(pointer_id) {
  wm::CaptureController::Get()->AddObserver(this);
  top_level_event_handler_->window()->AddObserver(this);
}

PointerPressHandler::~PointerPressHandler() {
  top_level_event_handler_->window()->RemoveObserver(this);
  wm::CaptureController::Get()->RemoveObserver(this);
}

void PointerPressHandler::OnCaptureChanged(aura::Window* lost_capture,
                                           aura::Window* gained_capture) {
  if (gained_capture != top_level_event_handler_->window())
    top_level_event_handler_->DestroyPointerPressHandler(pointer_id_);
}

void PointerPressHandler::OnWindowVisibilityChanged(aura::Window* window,
                                                    bool visible) {
  if (!top_level_event_handler_->window()->IsVisible())
    top_level_event_handler_->DestroyPointerPressHandler(pointer_id_);
}

}  // namespace

// WindowTargeter used for ProxyWindows. This is used for three purposes:
// . If the location is in the non-client area, then child Windows are not
//   considered. This is done to ensure the delegate of the window (which is
//   local) sees the event.
// . To ensure |WindowTree::intercepts_events_| is honored.
// . To support custom shaped windows through SetShape().
class ProxyWindow::ProxyWindowTargeter : public aura::WindowTargeter {
 public:
  explicit ProxyWindowTargeter(ProxyWindow* proxy_window)
      : proxy_window_(proxy_window) {}
  ~ProxyWindowTargeter() override = default;

  void SetShape(const std::vector<gfx::Rect>& shape) { shape_ = shape; }

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    // It's okay to check only when the window is the proxy_window. Any
    // descendants should pass this condition once it passes with proxy_window.
    if (window == proxy_window_->window() && !IsLocationInShape(event))
      return false;

    // If the top-level does not have insets, then forward the call to the
    // parent's WindowTargeter. This is necessary for targeters such as
    // EasyResizeWindowTargeter to work correctly.
    if (mouse_extend().IsEmpty() && touch_extend().IsEmpty() &&
        proxy_window_->IsTopLevel() && window->parent()) {
      aura::WindowTargeter* parent_targeter =
          static_cast<WindowTargeter*>(window->parent()->targeter());
      if (parent_targeter)
        return parent_targeter->SubtreeShouldBeExploredForEvent(window, event);
    }
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* event_target,
                                      ui::Event* event) override {
    aura::Window* window = static_cast<aura::Window*>(event_target);
    DCHECK_EQ(window, proxy_window_->window());
    if (proxy_window_->DoesOwnerInterceptEvents()) {
      // If the owner intercepts events, then don't recurse (otherwise events
      // would go to a descendant).
      return event_target->CanAcceptEvent(*event) ? window : nullptr;
    }

    // Ensure events in the non-client area target the top-level window.
    // TopLevelEventHandler will ensure these are routed correctly.
    if (event->IsLocatedEvent() &&
        IsLocationInNonClientArea(window,
                                  event->AsLocatedEvent()->location())) {
      return window;
    }
    return aura::WindowTargeter::FindTargetForEvent(event_target, event);
  }

 private:
  bool IsLocationInShape(const ui::LocatedEvent& event) {
    // If |shape_| is empty, the handling of custom shapes are not used. Always
    // return true.
    if (shape_.empty())
      return true;

    gfx::Point location = event.root_location();
    aura::Window::ConvertPointToTarget(proxy_window_->window()->GetRootWindow(),
                                       proxy_window_->window(), &location);
    for (const auto& rect : shape_) {
      if (rect.Contains(location))
        return true;
    }
    return false;
  }

  ProxyWindow* const proxy_window_;
  std::vector<gfx::Rect> shape_;

  DISALLOW_COPY_AND_ASSIGN(ProxyWindowTargeter);
};

ProxyWindow::~ProxyWindow() {
  // WindowTree/ClientRoot should have reset |attached_frame_sink_id_| before
  // the Window is destroyed.
  DCHECK(!attached_frame_sink_id_.is_valid());
}

// static
ProxyWindow* ProxyWindow::Create(aura::Window* window,
                                 WindowTree* tree,
                                 const viz::FrameSinkId& frame_sink_id,
                                 bool is_top_level) {
  DCHECK(!GetMayBeNull(window));
  // Owned by |window|.
  ProxyWindow* proxy_window =
      new ProxyWindow(window, tree, frame_sink_id, is_top_level);
  return proxy_window;
}

// static
const ProxyWindow* ProxyWindow::GetMayBeNull(const aura::Window* window) {
  return window ? window->GetProperty(kProxyWindowKey) : nullptr;
}

void ProxyWindow::Destroy() {
  // This should only be called for windows created locally for an embedding
  // (not created by a remote client). Such windows do not have an owner.
  DCHECK(!owning_window_tree_);
  // static_cast is needed to determine which function SetProperty() applies
  // to.
  window_->SetProperty(kProxyWindowKey, static_cast<ProxyWindow*>(nullptr));
}

WindowTree* ProxyWindow::embedded_window_tree() {
  return embedding_ ? embedding_->embedded_tree() : nullptr;
}

const WindowTree* ProxyWindow::embedded_window_tree() const {
  return embedding_ ? embedding_->embedded_tree() : nullptr;
}

void ProxyWindow::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  if (client_area_ == insets &&
      additional_client_areas == additional_client_areas_) {
    return;
  }

  additional_client_areas_ = additional_client_areas;
  client_area_ = insets;
}

void ProxyWindow::SetHitTestInsets(const gfx::Insets& mouse,
                                   const gfx::Insets& touch) {
  window_targeter_->SetInsets(mouse, touch);
}

void ProxyWindow::SetShape(const std::vector<gfx::Rect>& shape) {
  window_targeter_->SetShape(shape);
}

void ProxyWindow::SetCaptureOwner(WindowTree* owner) {
  capture_owner_ = owner;
  if (!IsTopLevel())
    return;

  static_cast<TopLevelEventHandler*>(event_handler_.get())
      ->OnCaptureOwnerChanged();
}

void ProxyWindow::StoreCursor(const ui::Cursor& cursor) {
  cursor_ = cursor;
}

bool ProxyWindow::DoesOwnerInterceptEvents() const {
  return embedding_ && embedding_->embedding_tree_intercepts_events();
}

void ProxyWindow::SetEmbedding(std::unique_ptr<Embedding> embedding) {
  embedding_ = std::move(embedding);
}

bool ProxyWindow::HasNonClientArea() const {
  return owning_window_tree_ && owning_window_tree_->IsTopLevel(window_) &&
         (!client_area_.IsEmpty() || !additional_client_areas_.empty());
}

bool ProxyWindow::IsTopLevel() const {
  return owning_window_tree_ && owning_window_tree_->IsTopLevel(window_);
}

void ProxyWindow::AttachCompositorFrameSink(
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  attached_compositor_frame_sink_ = true;
  viz::HostFrameSinkManager* host_frame_sink_manager =
      window_->env()->context_factory_private()->GetHostFrameSinkManager();
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(compositor_frame_sink), std::move(client));
}

void ProxyWindow::SetDragDropDelegate(
    std::unique_ptr<DragDropDelegate> drag_drop_delegate) {
  drag_drop_delegate_ = std::move(drag_drop_delegate);
}

void ProxyWindow::SetTopLevelProxyWindow(
    std::unique_ptr<TopLevelProxyWindowImpl> window) {
  top_level_proxy_window_ = std::move(window);
}

std::string ProxyWindow::GetIdForDebugging() {
  return owning_window_tree_
             ? owning_window_tree_->ClientWindowIdForWindow(window_).ToString()
             : frame_sink_id_.ToString();
}

ProxyWindow::ProxyWindow(aura::Window* window,
                         WindowTree* tree,
                         const viz::FrameSinkId& frame_sink_id,
                         bool is_top_level)
    : window_(window),
      owning_window_tree_(tree),
      frame_sink_id_(frame_sink_id) {
  window_->SetProperty(kProxyWindowKey, this);
  if (is_top_level)
    event_handler_ = std::make_unique<TopLevelEventHandler>(this);
  else
    event_handler_ = std::make_unique<ProxyWindowEventHandler>(this);
  auto proxy_window_targeter = std::make_unique<ProxyWindowTargeter>(this);
  window_targeter_ = proxy_window_targeter.get();
  window_->SetEventTargeter(std::move(proxy_window_targeter));
  // In order for a window to receive events it must have a target_handler()
  // (see Window::CanAcceptEvent()). Normally the delegate is the TargetHandler,
  // but if the delegate is null, then so is the target_handler(). Set
  // |event_handler_| as the target_handler() to force the Window to accept
  // events.
  if (!window_->delegate())
    window_->SetTargetHandler(event_handler_.get());
}

bool ProxyWindow::IsHandlingPointerPressForTesting(ui::PointerId pointer_id) {
  DCHECK(IsTopLevel());
  return static_cast<TopLevelEventHandler*>(event_handler_.get())
      ->IsHandlingPointerPress(pointer_id);
}

}  // namespace ws
