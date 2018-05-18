// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/client_window.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ui/ws2/window_host_frame_sink_client.h"
#include "services/ui/ws2/window_service_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event_handler.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::ws2::ClientWindow*);

namespace ui {
namespace ws2 {
namespace {
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ws2::ClientWindow,
                                   kClientWindowKey,
                                   nullptr);

// Returns true if |location| is in the non-client area (or outside the bounds
// of the window). A return value of false means the location is in the client
// area.
bool IsLocationInNonClientArea(const aura::Window* window,
                               const gfx::Point& location) {
  const ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  if (!client_window || !client_window->IsTopLevel())
    return false;

  // Locations outside the bounds, assume it's in extended hit test area, which
  // is non-client area.
  if (!gfx::Rect(window->bounds().size()).Contains(location))
    return true;

  gfx::Rect client_area(window->bounds().size());
  client_area.Inset(client_window->client_area());
  if (client_area.Contains(location))
    return false;

  for (const auto& rect : client_window->additional_client_areas()) {
    if (rect.Contains(location))
      return false;
  }
  return true;
}

// WindowTargeter used for ClientWindows. This is used for two purposes:
// . If the location is in the non-client area, then child Windows are not
//   considered. This is done to ensure the delegate of the window (which is
//   local) sees the event.
// . To ensure |WindowServiceClient::intercepts_events_| is honored.
class ClientWindowTargeter : public aura::WindowTargeter {
 public:
  explicit ClientWindowTargeter(ClientWindow* client_window)
      : client_window_(client_window) {}
  ~ClientWindowTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* event_target,
                                      ui::Event* event) override {
    aura::Window* window = static_cast<aura::Window*>(event_target);
    DCHECK_EQ(window, client_window_->window());
    if (client_window_->embedded_window_service_client() &&
        client_window_->owning_window_service_client() &&
        client_window_->owning_window_service_client()->intercepts_events()) {
      return event_target->CanAcceptEvent(*event) ? window : nullptr;
    }
    return aura::WindowTargeter::FindTargetForEvent(event_target, event);
  }

 private:
  ClientWindow* const client_window_;

  DISALLOW_COPY_AND_ASSIGN(ClientWindowTargeter);
};

// ClientWindowEventHandler is used to forward events to the client.
// ClientWindowEventHandler adds itself to the pre-phase to ensure it's
// considered before the Window's delegate (or other EventHandlers).
class ClientWindowEventHandler : public ui::EventHandler {
 public:
  explicit ClientWindowEventHandler(ClientWindow* client_window)
      : client_window_(client_window) {
    client_window->window()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }
  ~ClientWindowEventHandler() override {
    client_window_->window()->RemovePreTargetHandler(this);
  }

  ClientWindow* client_window() { return client_window_; }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // Because we StopPropagation() in the pre-phase an event should never be
    // received for other phases.
    DCHECK_EQ(event->phase(), EP_PRETARGET);
    // Events typically target the embedded client. Exceptions include when the
    // embedder intercepts events, or the window is a top-level and the event is
    // in the non-client area.
    auto* owning = client_window_->owning_window_service_client();
    auto* embedded = client_window_->embedded_window_service_client();
    auto* client = (owning && owning->intercepts_events()) || !embedded
                       ? owning
                       : embedded;
    DCHECK(client);
    client->SendEventToClient(client_window_->window(), *event);

    // The event was forwarded to the remote client. We don't want it handled
    // locally too.
    event->StopPropagation();
  }

 private:
  ClientWindow* const client_window_;

  DISALLOW_COPY_AND_ASSIGN(ClientWindowEventHandler);
};

// ui::EventHandler used for top-levels. Some events that target the non-client
// area are not sent to the client, instead are handled locally. For example,
// if a press occurs in the non-client area, then the event is not sent to
// the client, it's handled locally.
class TopLevelEventHandler : public ClientWindowEventHandler {
 public:
  explicit TopLevelEventHandler(ClientWindow* client_window)
      : ClientWindowEventHandler(client_window) {
    // Top-levels should always have an owning_window_service_client().
    // OnEvent() assumes this.
    DCHECK(client_window->owning_window_service_client());
  }

  ~TopLevelEventHandler() override = default;

  // ClientWindowEventHandler:
  void OnEvent(ui::Event* event) override {
    if (event->phase() != EP_PRETARGET) {
      // All work is done in the pre-phase. If this branch is hit, it means
      // event propagation was not stopped, and normal processing should
      // continue. Early out to avoid sending the event to the client again.
      return;
    }

    if (!event->IsLocatedEvent()) {
      ClientWindowEventHandler::OnEvent(event);
      return;
    }

    // This code does has two specific behaviors. It's used to ensure events
    // go to the right target (either local, or the remote client).
    // . a press-release sequence targets only one. If in non-client area then
    //   local, otherwise remote client.
    // . mouse-moves (not drags) go to both targets.
    // TODO(sky): handle touch events too.
    // TODO(sky): this also needs to handle capture changed and if the window
    // (or any ancestors) change visibility.
    bool stop_propagation = false;
    if (client_window()->HasNonClientArea() && event->IsMouseEvent()) {
      if (!mouse_down_state_) {
        if (event->type() == ui::ET_MOUSE_PRESSED) {
          mouse_down_state_ = std::make_unique<MouseDownState>();
          mouse_down_state_->in_non_client_area = IsLocationInNonClientArea(
              client_window()->window(), event->AsLocatedEvent()->location());
          if (mouse_down_state_->in_non_client_area)
            return;  // Don't send presses to client.
          stop_propagation = true;
        }
      } else {
        // Else case, in a press-release.
        const bool was_press_in_non_client_area =
            mouse_down_state_->in_non_client_area;
        if (event->type() == ui::ET_MOUSE_RELEASED &&
            event->AsMouseEvent()->button_flags() ==
                event->AsMouseEvent()->changed_button_flags()) {
          mouse_down_state_.reset();
        }
        if (was_press_in_non_client_area)
          return;  // Don't send release to client since press didn't go there.
        stop_propagation = true;
      }
    }
    client_window()->owning_window_service_client()->SendEventToClient(
        client_window()->window(), *event);
    if (stop_propagation)
      event->StopPropagation();
  }

 private:
  struct MouseDownState {
    bool in_non_client_area = false;
  };

  // Non-null while in a mouse press-drag-release cycle.
  std::unique_ptr<MouseDownState> mouse_down_state_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelEventHandler);
};

}  // namespace

ClientWindow::~ClientWindow() = default;

// static
ClientWindow* ClientWindow::Create(aura::Window* window,
                                   WindowServiceClient* client,
                                   const viz::FrameSinkId& frame_sink_id,
                                   bool is_top_level) {
  DCHECK(!GetMayBeNull(window));
  // Owned by |window|.
  ClientWindow* client_window =
      new ClientWindow(window, client, frame_sink_id, is_top_level);
  window->SetProperty(kClientWindowKey, client_window);
  return client_window;
}

// static
const ClientWindow* ClientWindow::GetMayBeNull(const aura::Window* window) {
  return window ? window->GetProperty(kClientWindowKey) : nullptr;
}

void ClientWindow::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  if (!window_host_frame_sink_client_) {
    window_host_frame_sink_client_ =
        std::make_unique<WindowHostFrameSinkClient>();
    host_frame_sink_manager->RegisterFrameSinkId(
        frame_sink_id_, window_host_frame_sink_client_.get());
    // TODO: need to unregister, and update on changes.
    // TODO: figure what parts of the ancestors (and descendants) need to be
    // registered.
  }

  host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id, window_host_frame_sink_client_.get());
  window_->SetEmbedFrameSinkId(frame_sink_id_);
  window_host_frame_sink_client_->OnFrameSinkIdChanged();
}

void ClientWindow::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  if (client_area_ == insets &&
      additional_client_areas == additional_client_areas_) {
    return;
  }

  additional_client_areas_ = additional_client_areas;
  client_area_ = insets;

  // TODO(sky): update cursor if over this window.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool ClientWindow::HasNonClientArea() const {
  return owning_window_service_client_ &&
         owning_window_service_client_->IsTopLevel(window_) &&
         (!client_area_.IsEmpty() || !additional_client_areas_.empty());
}

bool ClientWindow::IsTopLevel() const {
  return owning_window_service_client_ &&
         owning_window_service_client_->IsTopLevel(window_);
}

void ClientWindow::AttachCompositorFrameSink(
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(compositor_frame_sink), std::move(client));
}

ClientWindow::ClientWindow(aura::Window* window,
                           WindowServiceClient* client,
                           const viz::FrameSinkId& frame_sink_id,
                           bool is_top_level)
    : window_(window),
      owning_window_service_client_(client),
      frame_sink_id_(frame_sink_id) {
  if (is_top_level)
    event_handler_ = std::make_unique<TopLevelEventHandler>(this);
  else
    event_handler_ = std::make_unique<ClientWindowEventHandler>(this);
  window_->SetEventTargeter(std::make_unique<ClientWindowTargeter>(this));
  // In order for a window to receive events it must have a target_handler()
  // (see Window::CanAcceptEvent()). Normally the delegate is the TargetHandler,
  // but if the delegate is null, then so is the target_handler(). Set
  // |event_handler_| as the target_handler() to force the Window to accept
  // events.
  if (!window_->delegate())
    window_->SetTargetHandler(event_handler_.get());
}

}  // namespace ws2
}  // namespace ui
