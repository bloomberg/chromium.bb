// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_

namespace vr {

// TODO(vollick): add support for unloading the SDK.
bool LoadArCoreSdk(const std::string& libraryPath);

/**
 * Determines whether AR Core features are supported. Currently, this only
 * depends on the OS version, but could be more sophisticated.
 * Calling this method won't load AR Core SDK and does not depend
 * on AR Core SDK to be loaded.
 * Returns true if the AR Core usage is supported, false otherwise.
 */
bool SupportsArCore();

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_
