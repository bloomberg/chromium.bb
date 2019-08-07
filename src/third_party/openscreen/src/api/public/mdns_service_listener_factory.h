// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_MDNS_SERVICE_LISTENER_FACTORY_H_
#define API_PUBLIC_MDNS_SERVICE_LISTENER_FACTORY_H_

#include <memory>

#include "api/public/service_listener.h"

namespace openscreen {

struct MdnsServiceListenerConfig {
  // TODO(mfoltz): Populate with actual parameters as implementation progresses.
  bool dummy_value = true;
};

class MdnsServiceListenerFactory {
 public:
  static std::unique_ptr<ServiceListener> Create(
      const MdnsServiceListenerConfig& config,
      ServiceListener::Observer* observer);
};

}  // namespace openscreen

#endif  // API_PUBLIC_MDNS_SERVICE_LISTENER_FACTORY_H_
