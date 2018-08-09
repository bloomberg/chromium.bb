// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/injected_event_handler.h"

#include "base/memory/ptr_util.h"
#include "services/ui/ws2/window_service.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace ui {
namespace ws2 {

InjectedEventHandler::InjectedEventHandler(
    WindowService* window_service,
    aura::WindowTreeHost* window_tree_host)
    : window_service_(window_service), window_tree_host_(window_tree_host) {
  window_service_->AddObserver(this);
  window_tree_host_->window()->AddObserver(this);
  window_tree_host_->window()->env()->AddWindowEventDispatcherObserver(this);
}

InjectedEventHandler::~InjectedEventHandler() {
  RemoveObservers();
}

void InjectedEventHandler::Inject(std::unique_ptr<ui::Event> event,
                                  ResultCallback result_callback) {
  DCHECK(window_service_);  // Inject() can only be called once.
  DCHECK(!result_callback_);
  result_callback_ = std::move(result_callback);
  DCHECK(result_callback_);

  aura::Window* window_tree_host_window = window_tree_host_->window();
  window_tree_host_window->AddPreTargetHandler(
      this, EventTarget::Priority::kAccessibility);
  // No need to do anything with the result of sending the event.
  ignore_result(
      window_tree_host_->event_sink()->OnEventFromSource(event.get()));

  // WARNING: it's possible |this| has been destroyed. Make sure you don't
  // access any locals after this. The use of |this| here is safe as it's only
  // used to remove from a list.
  window_tree_host_window->RemovePreTargetHandler(this);
}

void InjectedEventHandler::NotifyCallback() {
  RemoveObservers();
  std::move(result_callback_).Run();
}

void InjectedEventHandler::RemoveObservers() {
  if (!window_service_)
    return;

  window_tree_host_->window()->env()->RemoveWindowEventDispatcherObserver(this);
  window_tree_host_->window()->RemoveObserver(this);
  window_service_->RemoveObserver(this);
  window_service_ = nullptr;
}

void InjectedEventHandler::OnWindowEventDispatcherFinishedProcessingEvent(
    aura::WindowEventDispatcher* dispatcher) {
  if (!event_id_ && dispatcher->host() == window_tree_host_ &&
      event_dispatched_) {
    // The WindowEventDispatcher finished processing and the event was not sent
    // to a remote client, notify the callback. This happens here rather than
    // OnEvent() as during OnEvent() we don't yet know if the event is going to
    // a remote client.
    NotifyCallback();
  }
}

void InjectedEventHandler::OnWindowEventDispatcherDispatchedHeldEvents(
    aura::WindowEventDispatcher* dispatcher) {
  if (!event_id_ && dispatcher->host() == window_tree_host_)
    NotifyCallback();
}

void InjectedEventHandler::OnWindowDestroying(aura::Window* window) {
  // This is called when the WindowTreeHost has been destroyed. Assume we won't
  // be getting an ack from the client.
  NotifyCallback();
}

void InjectedEventHandler::OnWillSendEventToClient(ClientSpecificId client_id,
                                                   uint32_t event_id) {
  if (event_id_)
    return;  // Already waiting.

  event_id_ = std::make_unique<EventId>();
  event_id_->client_id = client_id;
  event_id_->event_id = event_id;
}

void InjectedEventHandler::OnClientAckedEvent(ClientSpecificId client_id,
                                              uint32_t event_id) {
  if (event_id_ && event_id_->client_id == client_id &&
      event_id_->event_id == event_id) {
    NotifyCallback();
  }
}

void InjectedEventHandler::OnWillDestroyClient(ClientSpecificId client_id) {
  // If the client this is waiting on is being destroyed, an ack will never be
  // received.
  if (event_id_ && event_id_->client_id == client_id)
    NotifyCallback();
}

void InjectedEventHandler::OnEvent(Event* event) {
  // This is called if the event is actually going to be delivered to a target
  // (not held by WindowEventDispatcher). Don't call NotifyCallback() yet, as we
  // don't yet know if the event is going to a remote client.
  event_dispatched_ = true;
}

}  // namespace ws2
}  // namespace ui
