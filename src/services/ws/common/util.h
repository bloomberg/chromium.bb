// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_COMMON_UTIL_H_
#define SERVICES_WS_COMMON_UTIL_H_

#include <stdint.h>

#include "services/ws/common/types.h"

namespace ws {

inline ClientSpecificId ClientIdFromTransportId(Id id) {
  return static_cast<ClientSpecificId>((id >> 32) & 0xFFFFFFFF);
}

inline ClientSpecificId ClientWindowIdFromTransportId(Id id) {
  return static_cast<ClientSpecificId>(id & 0xFFFFFFFF);
}

inline Id BuildTransportId(ClientSpecificId connection_id,
                           ClientSpecificId window_id) {
  return (static_cast<Id>(connection_id) << 32) | static_cast<Id>(window_id);
}

}  // namespace ws

#endif  // SERVICES_WS_COMMON_UTIL_H_
