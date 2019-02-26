// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_channel.h"

#include "components/version_info/android/channel_getter.h"

namespace android_webview {

using version_info::Channel;

Channel GetChannelOrStable() {
  Channel channel = version_info::android::GetChannel();
  return channel == Channel::UNKNOWN ? Channel::STABLE : channel;
}

}  // namespace android_webview
