# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urllib


class PerfPlatform(object):
  def __init__(self, name, is_fyi=False):
    self._name = name
    self._is_fyi = is_fyi

  @property
  def name(self):
    return self._name

  @property
  def buildbot_url(self):
    return ('https://ci.chromium.org/buildbot/chromium.perf/%s/' %
             urllib.quote(self._name))

# Linux
LINUX = PerfPlatform('linux-perf')

# Mac
MAC_HIGH_END = PerfPlatform('mac-10_13_laptop_high_end-perf')
MAC_LOW_END = PerfPlatform('mac-10_12_laptop_low_end-perf')

# Win
WIN_10 = PerfPlatform('win-10-perf')
WIN_7 = PerfPlatform('Win 7 Perf')
WIN_7_GPU = PerfPlatform('Win 7 Nvidia GPU Perf')

# Android
ANDROID_GO = PerfPlatform('android-go-perf')
ANDROID_NEXUS_5 = PerfPlatform('Android Nexus5 Perf')
ANDROID_NEXUS_5X = PerfPlatform('android-nexus5x-perf')
ANDROID_NEXUS_5X_WEBVIEW = PerfPlatform('Android Nexus5X WebView Perf')
ANDROID_NEXUS_6_WEBVIEW = PerfPlatform('Android Nexus6 WebView Perf')

ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}

ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}
