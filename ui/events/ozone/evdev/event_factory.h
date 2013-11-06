// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_
#define UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_

#include "base/compiler_specific.h"
#include "ui/events/events_export.h"
#include "ui/events/ozone/evdev/event_modifiers.h"
#include "ui/events/ozone/event_factory_ozone.h"

namespace ui {

// Ozone events implementation for the Linux input subsystem ("evdev").
class EVENTS_EXPORT EventFactoryEvdev : public EventFactoryOzone {
 public:
  EventFactoryEvdev();
  virtual ~EventFactoryEvdev();

  virtual void StartProcessingEvents() OVERRIDE;

 private:
  EventModifiersEvdev modifiers_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_EVDEV_H_
