// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/error.h"

#include <errno.h>
#include <string.h>

namespace openscreen {
namespace platform {

int GetLastError() {
  return errno;
}

std::string GetLastErrorString() {
  return strerror(errno);
}

}  // namespace platform
}  // namespace openscreen
