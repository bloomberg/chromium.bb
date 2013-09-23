// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/event_converter_ozone.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_ozone.h"
#include "ui/events/event.h"

namespace {

void DispatchEventHelper(scoped_ptr<ui::Event> key) {
  base::MessagePumpOzone::Current()->Dispatch(key.get());
}

}  // namespace

namespace ui {

EventConverterOzone::EventConverterOzone() {
}

EventConverterOzone::~EventConverterOzone() {
}

void EventConverterOzone::DispatchEvent(scoped_ptr<ui::Event> event) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&DispatchEventHelper, base::Passed(&event)));
}

}  // namespace ui
