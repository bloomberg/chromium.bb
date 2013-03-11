// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/extensions/event_base.h"

#include "ppapi/cpp/extensions/dev/events_dev.h"

namespace pp {
namespace ext {
namespace internal {

GenericEventBase::GenericEventBase(
    const InstanceHandle& instance,
    const PP_Ext_EventListener& pp_listener)
    : instance_(instance),
      listener_id_(0),
      pp_listener_(pp_listener) {
}

GenericEventBase::~GenericEventBase() {
  StopListening();
}

bool GenericEventBase::StartListening() {
  if (IsListening())
    return true;

  listener_id_ = events::Events_Dev::AddListener(instance_.pp_instance(),
                                                 pp_listener_);
  return IsListening();
}

void GenericEventBase::StopListening() {
  if (!IsListening())
    return;

  events::Events_Dev::RemoveListener(instance_.pp_instance(), listener_id_);
  listener_id_ = 0;
}

}  // namespace internal
}  // namespace ext
}  // namespace pp
