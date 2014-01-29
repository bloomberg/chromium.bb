// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_CONVERTER_OZONE_H_
#define UI_EVENTS_OZONE_EVENT_CONVERTER_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/events_export.h"

namespace ui {
class Event;

// Ozone implementations can generate input events in whatever way is
// appropriate for the platform.
// This class provides the functionality needed in common across all
// converters: dispatching the |ui::Event| to aura.
class EVENTS_EXPORT EventConverterOzone {
 public:
  EventConverterOzone();
  virtual ~EventConverterOzone();

 protected:
  // Subclasses should use this method to post a task that will dispatch
  // |event| from the UI message loop. This method takes ownership of
  // |event|. |event| will be deleted at the end of the posted task.
  virtual void DispatchEvent(scoped_ptr<ui::Event> event);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventConverterOzone);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_CONVERTER_OZONE_H_
