# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '../webrtc_overrides/field_trial.cc',
  ],
  'conditions': [
    ['OS=="win"', {
      'msvs_disabled_warnings': [
        4005,  # WIN32_LEAN_AND_MEAN.
        4267,  # size_t to int truncations.
      ],
    }],
  ],
}
