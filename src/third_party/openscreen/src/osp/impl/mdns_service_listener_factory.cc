// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/mdns_service_listener_factory.h"

#include "osp/impl/internal_services.h"

namespace openscreen {

class TaskRunner;

namespace osp {

// static
std::unique_ptr<ServiceListener> MdnsServiceListenerFactory::Create(
    const MdnsServiceListenerConfig& config,
    ServiceListener::Observer* observer,
    TaskRunner* task_runner) {
  return InternalServices::CreateListener(config, observer, task_runner);
}

}  // namespace osp
}  // namespace openscreen
