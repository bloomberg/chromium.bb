# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from measurements import media
from telemetry import test

class Media(test.Test):
  """Obtains media metrics for key user scenarios."""
  test = media.Media
  page_set = 'page_sets/tough_video_cases.json'

class MediaNetworkSimulation(test.Test):
  """Obtains media metrics under different network simulations."""
  test = media.Media
  enabled = not sys.platform.startswith('linux')
  page_set = 'page_sets/media_cns_cases.json'

class MediaAndroid(test.Test):
  """Obtains media metrics for key user scenarios on Android."""
  test = media.Media
  tag = 'android'
  page_set = 'page_sets/tough_video_cases.json'
  # Exclude crowd* media files (50fps 2160p).
  options = {
      'page_filter_exclude': '.*crowd.*'
  }

  def CustomizeBrowserOptions(self, options):
    # Needed to run media actions in JS in Android.
    options.AppendExtraBrowserArgs(
        '--disable-gesture-requirement-for-media-playback')

class MediaSourceExtensions(test.Test):
  """Obtains media metrics for key media source extensions functions."""
  test = media.Media
  enabled = not sys.platform.startswith('linux')
  page_set = 'page_sets/mse_cases.json'

  def CustomizeBrowserOptions(self, options):
    # Needed to allow XHR requests to return stream objects.
    options.AppendExtraBrowserArgs(
        '--enable-experimental-web-platform-features')

