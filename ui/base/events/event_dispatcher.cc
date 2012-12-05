// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_dispatcher.h"

#include <algorithm>

namespace ui {

EventDispatcher::EventDispatcher()
    : set_on_destroy_(NULL),
      current_event_(NULL),
      handler_list_(NULL) {
}

EventDispatcher::~EventDispatcher() {
  if (set_on_destroy_)
    *set_on_destroy_ = true;
}

void EventDispatcher::OnHandlerDestroyed(EventHandler* handler) {
  if (!handler_list_)
    return;
  handler_list_->erase(std::find(handler_list_->begin(),
                                 handler_list_->end(),
                                 handler));
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher, private:

void EventDispatcher::DispatchEventToSingleHandler(EventHandler* handler,
                                                   Event* event) {
  handler->OnEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher::ScopedDispatchHelper

EventDispatcher::ScopedDispatchHelper::ScopedDispatchHelper(Event* event)
    : Event::DispatcherApi(event) {
  set_result(ui::ER_UNHANDLED);
}

EventDispatcher::ScopedDispatchHelper::~ScopedDispatchHelper() {
  set_phase(EP_POSTDISPATCH);
}

}  // namespace ui
