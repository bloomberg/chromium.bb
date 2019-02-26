// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_CHANNEL_H_
#define ANDROID_WEBVIEW_COMMON_AW_CHANNEL_H_

#include "components/version_info/version_info.h"

namespace android_webview {

// There are separate Monochrome APKs built for each channel, but only one
// stand-alone WebView APK for all channels, so stand-alone WebView has channel
// "unknown". Return the channel if it's known, or "stable" if it's "unknown".
version_info::Channel GetChannelOrStable();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_CHANNEL_H_
