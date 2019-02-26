// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_queue.h"

#include "base/stl_util.h"
#include "services/ws/host_event_dispatcher.h"
#include "services/ws/host_event_queue.h"
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
  HostEventQueue* host = nullptr;
  std::unique_ptr<ui::Event> event;
  bool honor_rewriters = false;

  base::OnceClosure callback;
};

EventQueue::EventQueue(WindowService* service) : window_service_(service) {
  window_service_->AddObserver(this);
}

EventQueue::~EventQueue() {
  window_service_->RemoveObserver(this);
}

std::unique_ptr<HostEventQueue> EventQueue::RegisterHostEventDispatcher(
    aura::WindowTreeHost* window_tree_host,
    HostEventDispatcher* dispatcher) {
  return std::make_unique<HostEventQueue>(weak_factory_.GetWeakPtr(),
                                          window_tree_host, dispatcher);
}

// static
void EventQueue::DispatchOrQueueEvent(WindowService* service,
                                      aura::WindowTreeHost* window_tree_host,
                                      ui::Event* event,
                                      bool honor_rewriters) {
  DCHECK(window_tree_host);
  HostEventQueue* host_event_queue =
      service->event_queue()->GetHostEventQueueForDisplay(
          window_tree_host->GetDisplayId());
  DCHECK(host_event_queue);
  host_event_queue->DispatchOrQueueEvent(event, honor_rewriters);
}

bool EventQueue::ShouldQueueEvent(HostEventQueue* host_queue,
                                  const ui::Event& event) {
  if (!in_flight_event_ || !IsQueableEvent(event))
    return false;
  aura::WindowTargeter* targeter =
      host_queue->window_tree_host()->window()->targeter();
  if (!targeter)
    targeter = host_queue->window_tree_host()->dispatcher()->event_targeter();
  DCHECK(targeter);
  aura::Window* target =
      targeter->FindTargetForKeyEvent(host_queue->window_tree_host()->window());
  return target && WindowService::HasRemoteClient(target);
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

HostEventQueue* EventQueue::GetHostEventQueueForDisplay(int64_t display_id) {
  for (HostEventQueue* host_queue : host_event_queues_) {
    if (host_queue->window_tree_host()->GetDisplayId() == display_id)
      return host_queue;
  }
  return nullptr;
}

void EventQueue::OnClientTookTooLongToAckEvent() {
  DVLOG(1) << "Client took too long to respond to event";
  DCHECK(in_flight_event_);
  OnClientAckedEvent(in_flight_event_->client_id, in_flight_event_->event_id);
}

void EventQueue::OnHostEventQueueCreated(HostEventQueue* host) {
  host_event_queues_.insert(host);
}

void EventQueue::OnHostEventQueueDestroyed(HostEventQueue* host) {
  base::EraseIf(queued_events_,
                [&host](const std::unique_ptr<QueuedEvent>& event) {
                  return event->host == host;
                });
  host_event_queues_.erase(host);
}

void EventQueue::QueueEvent(HostEventQueue* host,
                            const ui::Event& event,
                            bool honor_rewriters) {
  DCHECK(ShouldQueueEvent(host, event));
  std::unique_ptr<QueuedEvent> queued_event = std::make_unique<QueuedEvent>();
  queued_event->host = host;
  queued_event->event = ui::Event::Clone(event);
  queued_event->honor_rewriters = honor_rewriters;
  queued_events_.push_back(std::move(queued_event));
}

void EventQueue::DispatchNextQueuedEvent() {
  while (!queued_events_.empty() && !in_flight_event_) {
    std::unique_ptr<QueuedEvent> queued_event =
        std::move(*queued_events_.begin());
    queued_events_.pop_front();
    if (queued_event->callback) {
      std::move(queued_event->callback).Run();
    } else {
      queued_event->host->DispatchEventDontQueue(queued_event->event.get(),
                                                 queued_event->honor_rewriters);
    }
  }
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
