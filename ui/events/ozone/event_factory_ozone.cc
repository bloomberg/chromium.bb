// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/event_factory_ozone.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event.h"
#include "ui/events/event_switches.h"

namespace ui {

// static
EventFactoryOzone* EventFactoryOzone::impl_ = NULL;

EventFactoryOzone::EventFactoryOzone() {}

EventFactoryOzone::~EventFactoryOzone() {}

EventFactoryOzone* EventFactoryOzone::GetInstance() {
  CHECK(impl_) << "No EventFactoryOzone implementation set.";
  return impl_;
}

void EventFactoryOzone::SetInstance(EventFactoryOzone* impl) { impl_ = impl; }

void EventFactoryOzone::StartProcessingEvents() {}

void EventFactoryOzone::SetFileTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {}

void EventFactoryOzone::WarpCursorTo(gfx::AcceleratedWidget widget,
                                     const gfx::PointF& location) {
  NOTIMPLEMENTED();
}

}  // namespace ui
