# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urllib


class PerfPlatform(object):
  def __init__(self, name, description, is_fyi=False):
    self._name = name
    self._description = description
    # For sorting ignore case and "segments" in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi

  def __lt__(self, other):
    if not isinstance(other, type(self)):
      return NotImplemented
    # pylint: disable=protected-access
    return self._sort_key < other._sort_key

  @property
  def name(self):
    return self._name

  @property
  def description(self):
    return self._description

  @property
  def platform(self):
    value = self._sort_key.split(' ', 1)[0]
    return 'windows' if value == 'win' else value

  @property
  def is_fyi(self):
    return self._is_fyi

  @property
  def buildbot_url(self):
    return ('https://ci.chromium.org/buildbot/chromium.perf/%s/' %
             urllib.quote(self._name))

# Linux
LINUX = PerfPlatform(
    'linux-perf', 'Ubuntu-14.04, 8 core, NVIDIA Quadro P400')

# Mac
MAC_HIGH_END = PerfPlatform(
    'mac-10_13_laptop_high_end-perf',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55')
MAC_LOW_END = PerfPlatform(
    'mac-10_12_laptop_low_end-perf',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics')

# Win
WIN_10 = PerfPlatform(
    'win-10-perf',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630')
WIN_7 = PerfPlatform('Win 7 Perf', 'N/A')
WIN_7_GPU = PerfPlatform('Win 7 Nvidia GPU Perf', 'N/A')

# Android
ANDROID_GO = PerfPlatform('android-go-perf', 'Android O')
ANDROID_NEXUS_5 = PerfPlatform('Android Nexus5 Perf', 'Android KOT49H')
ANDROID_NEXUS_5X = PerfPlatform('android-nexus5x-perf', 'Android MMB29Q')
ANDROID_NEXUS_5X_WEBVIEW = PerfPlatform(
    'Android Nexus5X WebView Perf', 'Android AOSP MOB30K')
ANDROID_NEXUS_6_WEBVIEW = PerfPlatform(
    'Android Nexus6 WebView Perf', 'Android AOSP MOB30K')

ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}

ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}
