# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import urllib

from core import benchmark_finders


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')

_ALL_BENCHMARKS_BY_NAMES= dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())

_OFFICIAL_BENCHMARKS = frozenset(
    benchmark_finders.GetOfficialBenchmarks())

def _IsPlatformSupported(benchmark, platform):
    supported = benchmark.GetSupportedPlatformNames(
        benchmark.SUPPORTED_PLATFORMS)
    return 'all' in supported or platform in supported


class PerfPlatform(object):
  def __init__(self, name, description, benchmark_names,
               is_fyi=False, num_shards=None):
    self._name = name
    self._description = description
    # For sorting ignore case and "segments" in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi
    assert num_shards
    self._num_shards = num_shards
    benchmarks = []
    for benchmark_name in benchmark_names:
      benchmarks.append(_ALL_BENCHMARKS_BY_NAMES[benchmark_name])
    benchmarks_to_run = frozenset(benchmarks)
    platform = self._sort_key.split(' ', 1)[0]
    # pylint: disable=redefined-outer-name
    self._benchmarks_to_run = frozenset([
        b for b in benchmarks_to_run if _IsPlatformSupported(b, platform)])
    # pylint: enable=redefined-outer-name

    base_file_name = name.replace(' ', '_').lower()
    self._timing_file_path = os.path.join(
        _SHARD_MAP_DIR, 'timing_data', base_file_name + '_timing.json')
    self._shards_map_file_path = os.path.join(
        _SHARD_MAP_DIR, base_file_name + '_map.json')

  def __lt__(self, other):
    if not isinstance(other, type(self)):
      return NotImplemented
    # pylint: disable=protected-access
    return self._sort_key < other._sort_key

  @property
  def num_shards(self):
    return self._num_shards

  @property
  def shards_map_file_path(self):
    return self._shards_map_file_path

  @property
  def timing_file_path(self):
    return self._timing_file_path

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
  def benchmarks_to_run(self):
    return self._benchmarks_to_run

  @property
  def is_fyi(self):
    return self._is_fyi

  @property
  def buildbot_url(self):
    return ('https://ci.chromium.org/p/chrome/builders/luci.chrome.ci/%s' %
             urllib.quote(self._name))


_OFFICIAL_BENCHMARK_NAMES = frozenset(
    [b.Name() for b in _OFFICIAL_BENCHMARKS])

_ANDROID_GO_BENCHMARK_NAMES = frozenset([
    'memory.top_10_mobile',
    'system_health.memory_mobile',
    'system_health.common_mobile',
    'power.typical_10_mobile',
    'startup.mobile',
    'system_health.webview_startup',
    'v8.browsing_mobile',
    'speedometer',
    'speedometer2'])

_ANDROID_NEXUS5X_FYI_BENCHMARK_NAMES = frozenset([
    'heap_profiling.mobile.disabled',
    'heap_profiling.mobile.native',
    'heap_profiling.mobile.pseudo'])

# Linux
LINUX = PerfPlatform(
    'linux-perf', 'Ubuntu-14.04, 8 core, NVIDIA Quadro P400',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=26)

# Mac
MAC_HIGH_END = PerfPlatform(
    'mac-10_13_laptop_high_end-perf',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=26)

MAC_LOW_END = PerfPlatform(
    'mac-10_12_laptop_low_end-perf',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=26)

# Win
WIN_10 = PerfPlatform(
    'win-10-perf',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630', _OFFICIAL_BENCHMARK_NAMES,
    num_shards=26)

WIN_7 = PerfPlatform(
    'Win 7 Perf', 'N/A', _OFFICIAL_BENCHMARK_NAMES,
    num_shards=5)

WIN_7_GPU = PerfPlatform(
    'Win 7 Nvidia GPU Perf', 'N/A', _OFFICIAL_BENCHMARK_NAMES,
    num_shards=5)

# Android
ANDROID_ARM_BUILDER = PerfPlatform(
    'android-builder-perf',
    'Static analysis of 32-bit ARM Android build products',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=1)

ANDROID_ARM64_BUILDER = PerfPlatform(
    'android_arm64-builder-perf',
    'Static analysis of 64-bit ARM Android build products.',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=1)

ANDROID_GO = PerfPlatform(
    'android-go-perf', 'Android O (gobo)', _ANDROID_GO_BENCHMARK_NAMES,
    num_shards=19)

ANDROID_GO_WEBVIEW = PerfPlatform(
    'android-go_webview-perf', 'Android OPM1.171019.021 (gobo)',
    _ANDROID_GO_BENCHMARK_NAMES, num_shards=25)

ANDROID_NEXUS_5 = PerfPlatform(
    'Android Nexus5 Perf', 'Android KOT49H', _OFFICIAL_BENCHMARK_NAMES,
    num_shards=16)

ANDROID_NEXUS_5X = PerfPlatform(
    'android-nexus5x-perf', 'Android MMB29Q', _OFFICIAL_BENCHMARK_NAMES,
    num_shards=16)

ANDROID_NEXUS_5X_WEBVIEW = PerfPlatform(
    'Android Nexus5X WebView Perf', 'Android AOSP MOB30K',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=16)

ANDROID_NEXUS_6_WEBVIEW = PerfPlatform(
    'Android Nexus6 WebView Perf', 'Android AOSP MOB30K',
    _OFFICIAL_BENCHMARK_NAMES,
    num_shards=12)  # Reduced from 16 per crbug.com/891848.

# FYI bots
ANDROID_PIXEL2 = PerfPlatform(
    'android-pixel2-perf', 'Android OPM1.171019.021',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=7, is_fyi=True)

ANDROID_PIXEL2_WEBVIEW = PerfPlatform(
    'android-pixel2_webview-perf', 'Android OPM1.171019.021',
    _OFFICIAL_BENCHMARK_NAMES, num_shards=7, is_fyi=True)

ANDROID_NEXUS5X_PERF_FYI =  PerfPlatform(
    'android-nexus5x-perf-fyi', 'Android MMB29Q',
    _ANDROID_NEXUS5X_FYI_BENCHMARK_NAMES,
    num_shards=3, is_fyi=True)

# TODO(crbug.com/902089): Add linux-perf-fyi once the bot is configured to use
# the sharding map.

ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}

FYI_PLATFORMS = {
    p for p in ALL_PLATFORMS if p.is_fyi
}

OFFICIAL_PLATFORMS = {
    p for p in ALL_PLATFORMS if not p.is_fyi
}

ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}

OFFICIAL_PLATFORM_NAMES = {
    p.name for p in OFFICIAL_PLATFORMS
}
