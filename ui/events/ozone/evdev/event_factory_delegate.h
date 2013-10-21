// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_
#define UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_

#include "base/compiler_specific.h"
#include "ui/events/ozone/event_factory_delegate_ozone.h"

namespace ui {

// Ozone events implementation for the Linux input subsystem ("evdev").
class EventFactoryDelegateEvdev : public EventFactoryDelegateOzone {
 public:
  EventFactoryDelegateEvdev();
  virtual ~EventFactoryDelegateEvdev();

  virtual void CreateStartupEventConverters(
      EventFactoryOzone* factory) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventFactoryDelegateEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_
