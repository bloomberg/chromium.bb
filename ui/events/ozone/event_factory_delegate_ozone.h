// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_OZONE_H_
#define UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_OZONE_H_

namespace ui {
class Event;
class EventFactoryOzone;

// An embedder can install an instance of this interface to take control of
// what |EventConverterOzone| objects get created on the creation of
// the |RootWindowHostOzone| object.
class EVENTS_EXPORT EventFactoryDelegateOzone {
 public:
  EventFactoryDelegateOzone();
  virtual ~EventFactoryDelegateOzone();

  // Override this method with embedder-appropriate converter creation.
  virtual void CreateStartupEventConverters(EventFactoryOzone* factory) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventFactoryDelegateOzone);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_FACTORY_DELEGATE_OZONE_H_
