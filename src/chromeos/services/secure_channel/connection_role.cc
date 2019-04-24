// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/connection_role.h"

namespace chromeos {

namespace secure_channel {

std::ostream& operator<<(std::ostream& stream, const ConnectionRole& role) {
  switch (role) {
    case ConnectionRole::kInitiatorRole:
      stream << "[initiator role]";
      break;
    case ConnectionRole::kListenerRole:
      stream << "[listener role]";
      break;
  }
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
