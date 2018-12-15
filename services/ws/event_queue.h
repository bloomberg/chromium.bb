// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_EVENT_QUEUE_H_
#define SERVICES_WS_EVENT_QUEUE_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "services/ws/ids.h"
#include "services/ws/window_service_observer.h"

namespace aura {
class WindowTreeHost;
}

namespace ui {
class Event;
struct EventDispatchDetails;
}

namespace ws {

class WindowService;

// EventQueue handles queueing, waiting, and delivering events to event sinks.
// EventQueue tracks the dispatch by way of being a WindowServiceObserver.
// EventQueue also supports queueing callbacks run in order with queued events.
class COMPONENT_EXPORT(WINDOW_SERVICE) EventQueue
    : public WindowServiceObserver {
 public:
  explicit EventQueue(WindowService* service);
  ~EventQueue() override;

  // Queue the event if needed, or deliver it directly to the host's event sink.
  // Call WindowTreeHost::SendEventToSink instead, if the event should be passed
  // through the host's EventRewriters; that calls through here after rewriting.
  base::Optional<ui::EventDispatchDetails> DeliverOrQueueEvent(
      aura::WindowTreeHost* host,
      ui::Event* event);

  // Notifies |closure| when this EventQueue is ready to dispatch an event,
  // which may be immediately.
  void NotifyWhenReadyToDispatch(base::OnceClosure closure);

 private:
  friend class EventQueueTestHelper;

  struct QueuedEvent;

  // Tracks an event that was sent to a client.
  struct InFlightEvent {
    ClientSpecificId client_id = 0u;
    uint32_t event_id = 0u;
  };

  // Returns true if |event| should be queued at this time for the given |host|.
  bool ShouldQueueEvent(aura::WindowTreeHost* host, const ui::Event& event);

  // Adds |event| to |queued_events_| for delivery to |host|.
  void QueueEvent(aura::WindowTreeHost* host, const ui::Event& event);

  // Called by |ack_timer_| if the client does not respond to the event in a
  // timely manner.
  void OnClientTookTooLongToAckEvent();

  // Processes QueuedEvents until |queued_events_| is empty, or there is an
  // event sent to a client.
  void DispatchNextQueuedEvent();

  // WindowServiceObserver:
  void OnWillSendEventToClient(ClientSpecificId client_id,
                               uint32_t event_id,
                               const ui::Event& event) override;
  void OnClientAckedEvent(ClientSpecificId client_id,
                          uint32_t event_id) override;
  void OnWillDestroyClient(ClientSpecificId client_id) override;

  WindowService* window_service_;

  base::circular_deque<std::unique_ptr<QueuedEvent>> queued_events_;

  // Set when an event has been sent to a client.
  base::Optional<InFlightEvent> in_flight_event_;

  base::OneShotTimer ack_timer_;

  // Because of destruction order HostEventQueues may outlive this.
  base::WeakPtrFactory<EventQueue> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventQueue);
};

}  // namespace ws

#endif  // SERVICES_WS_EVENT_QUEUE_H_
