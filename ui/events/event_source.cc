// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_source.h"

#include "ui/events/event_processor.h"

namespace ui {

EventDispatchDetails EventSource::SendEventToProcessor(Event* event) {
  EventProcessor* processor = GetEventProcessor();
  CHECK(processor);
  return processor->OnEventFromSource(event);
}

}  // namespace ui
