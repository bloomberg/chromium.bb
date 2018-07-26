// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_ERROR_H_
#define PLATFORM_API_ERROR_H_

#include <string>

namespace openscreen {
namespace platform {

int GetLastError();
std::string GetLastErrorString();

}  // namespace platform
}  // namespace openscreen

#endif
