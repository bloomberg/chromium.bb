// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_FACTORY_OZONE_H_
#define UI_EVENTS_OZONE_EVENT_FACTORY_OZONE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/events_export.h"
#include "ui/events/ozone/event_converter_ozone.h"

namespace ui {

class EventFactoryDelegateOzone;

// Creates and dispatches |ui.Event|'s. Ozone assumes that events arrive on file
// descriptors with one  |EventConverterOzone| instance for each descriptor.
// Ozone presumes that the set of file desctiprtors can vary at runtime so this
// class supports dynamically adding and removing |EventConverterOzone|
// instances as necessary.
class EVENTS_EXPORT EventFactoryOzone {
 public:
  EventFactoryOzone();
  ~EventFactoryOzone();

  // Sets an optional delegate responsible for creating the starting set of
  // EventConvertOzones under management. This permits embedders to override the
  // Linux /dev/input/*-based default as desired. The caller must manage the
  // scope of this object.
  static void SetEventFactoryDelegateOzone(EventFactoryDelegateOzone* delegate);

  // Called from RootWindowHostOzone to create the starting set of event
  // converters.
  void CreateStartupEventConverters();

  // Add an |EventConverterOzone| instances for the given file descriptor.
  // Transfers ownership of the |EventConverterOzone| to this class.
  void AddEventConverter(int fd, scoped_ptr<EventConverterOzone> converter);

  // Remote the |EventConverterOzone|
  void RemoveEventConverter(int fd);

 private:
  // |EventConverterOzone| for each file descriptor.
  typedef EventConverterOzone* Converter;
  typedef base::MessagePumpLibevent::FileDescriptorWatcher* FDWatcher;
  std::map<int, Converter> converters_;
  std::map<int, FDWatcher> watchers_;

  static EventFactoryDelegateOzone* delegate_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryOzone);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_FACTORY_OZONE_H_
