// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_SESSION_UTILS_ANDROID_H_
#define SYNC_UTIL_SESSION_UTILS_ANDROID_H_
#pragma once

#include <string>

namespace syncer {
namespace internal {

// Return the unique identifier of this device.
std::string GetAndroidId();

// Return the end-user-visible name for this device.
std::string GetModel();

// Return if the device is a tablet.
bool IsTabletUi();

}  // namespace internal
}  // namespace syncer

#endif  // SYNC_UTIL_SESSION_UTILS_ANDROID_H_
