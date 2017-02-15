// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_ANDROID_REGISTER_JNI_H_
#define SERVICES_DEVICE_ANDROID_REGISTER_JNI_H_

namespace device {

// Register all JNI bindings necessary for device service.
// Is expected to be called on initialization of device service.
bool EnsureJniRegistered();

}  // namespace device

#endif  // SERVICES_DEVICE_ANDROID_REGISTER_JNI_H_
