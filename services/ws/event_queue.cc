// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_queue.h"

#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"

namespace ws {
namespace {

constexpr base::TimeDelta kEventAckTimeout =
#if defined(NDEBUG)
    base::TimeDelta::FromMilliseconds(100);
#else
    base::TimeDelta::FromMilliseconds(1000);
#endif

bool IsQueableEvent(const ui::Event& event) {
  return event.IsKeyEvent();
}

}  // namespace

struct EventQueue::QueuedEvent {
  base::WeakPtr<aura::WindowTreeHost> host;  // May be destroyed while queued.
  std::unique_ptr<ui::Event> event;
  base::OnceClosure callback;
};

EventQueue::EventQueue(WindowService* service) : window_service_(service) {
  window_service_->AddObserver(this);
}

EventQueue::~EventQueue() {
  window_service_->RemoveObserver(this);
}

base::Optional<ui::EventDispatchDetails> EventQueue::DeliverOrQueueEvent(
    aura::WindowTreeHost* host,
    ui::Event* event) {
  if (!ShouldQueueEvent(host, *event))
    return host->GetEventSink()->OnEventFromSource(event);

  QueueEvent(host, *event);
  return base::nullopt;
}

void EventQueue::NotifyWhenReadyToDispatch(base::OnceClosure closure) {
  if (!in_flight_event_.has_value()) {
    std::move(closure).Run();
  } else {
    std::unique_ptr<QueuedEvent> queued_event = std::make_unique<QueuedEvent>();
    queued_event->callback = std::move(closure);
    queued_events_.push_back(std::move(queued_event));
  }
}

bool EventQueue::ShouldQueueEvent(aura::WindowTreeHost* host,
                                  const ui::Event& event) {
  if (!in_flight_event_ || !IsQueableEvent(event))
    return false;
  aura::WindowTargeter* targeter = host->window()->targeter();
  if (!targeter)
    targeter = host->dispatcher()->event_targeter();
  DCHECK(targeter);
  aura::Window* target = targeter->FindTargetForKeyEvent(host->window());
  return target && WindowService::IsProxyWindow(target);
}

void EventQueue::QueueEvent(aura::WindowTreeHost* host,
                            const ui::Event& event) {
  DCHECK(ShouldQueueEvent(host, event));
  std::unique_ptr<QueuedEvent> queued_event = std::make_unique<QueuedEvent>();
  queued_event->host = host->GetWeakPtr();
  queued_event->event = ui::Event::Clone(event);
  queued_events_.push_back(std::move(queued_event));
}

void EventQueue::DispatchNextQueuedEvent() {
  while (!queued_events_.empty() && !in_flight_event_) {
    std::unique_ptr<QueuedEvent> queued_event =
        std::move(*queued_events_.begin());
    queued_events_.pop_front();
    if (queued_event->callback) {
      DCHECK(!queued_event->event) << "Running callback and ignoring event";
      std::move(queued_event->callback).Run();
    } else if (queued_event->host) {
      ignore_result(queued_event->host->GetEventSink()->OnEventFromSource(
          queued_event->event.get()));
    }
  }
}

void EventQueue::OnClientTookTooLongToAckEvent() {
  DVLOG(1) << "Client took too long to respond to event";
  DCHECK(in_flight_event_);
  OnClientAckedEvent(in_flight_event_->client_id, in_flight_event_->event_id);
}

void EventQueue::OnWillSendEventToClient(ClientSpecificId client_id,
                                         uint32_t event_id,
                                         const ui::Event& event) {
  if (IsQueableEvent(event)) {
    DCHECK(!in_flight_event_);
    in_flight_event_ = InFlightEvent();
    in_flight_event_->client_id = client_id;
    in_flight_event_->event_id = event_id;
    ack_timer_.Start(FROM_HERE, kEventAckTimeout, this,
                     &EventQueue::OnClientTookTooLongToAckEvent);
  }
}

void EventQueue::OnClientAckedEvent(ClientSpecificId client_id,
                                    uint32_t event_id) {
  if (!in_flight_event_ || in_flight_event_->client_id != client_id ||
      in_flight_event_->event_id != event_id) {
    return;
  }
  in_flight_event_.reset();
  ack_timer_.Stop();
  DispatchNextQueuedEvent();
}

void EventQueue::OnWillDestroyClient(ClientSpecificId client_id) {
  if (in_flight_event_ && in_flight_event_->client_id == client_id)
    OnClientAckedEvent(client_id, in_flight_event_->event_id);
}

}  // namespace ws
