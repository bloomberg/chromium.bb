#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import os

_PATCH = 'cr3'
_LATEST_VERSION = '11-robolectric-6757853.' + _PATCH
_ROBO_URL_FILES = {
    'android-all-11-robolectric-6757853.jar':
        'https://repo.maven.apache.org/maven2/org/robolectric/android-all/11-robolectric-6757853/android-all-11-robolectric-6757853.jar',
    'android-all-10-robolectric-5803371.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/10-robolectric-5803371/android-all-10-robolectric-5803371.jar',
    'android-all-9-robolectric-4913185-2.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/9-robolectric-4913185-2/android-all-9-robolectric-4913185-2.jar',
    'android-all-8.1.0-robolectric-4611349.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/8.1.0-robolectric-4611349/android-all-8.1.0-robolectric-4611349.jar',
    'android-all-8.0.0_r4-robolectric-r1.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/8.0.0_r4-robolectric-r1/android-all-8.0.0_r4-robolectric-r1.jar',
    'android-all-7.1.0_r7-robolectric-r1.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/7.1.0_r7-robolectric-r1/android-all-7.1.0_r7-robolectric-r1.jar',
    'android-all-6.0.1_r3-robolectric-r1.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/6.0.1_r3-robolectric-r1/android-all-6.0.1_r3-robolectric-r1.jar',
    'android-all-5.0.2_r3-robolectric-r0.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/5.0.2_r3-robolectric-r0/android-all-5.0.2_r3-robolectric-r0.jar',
    'android-all-4.4_r1-robolectric-r2.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/4.4_r1-robolectric-r2/android-all-4.4_r1-robolectric-r2.jar',
}

def do_latest():
  print(_LATEST_VERSION)


def _get_download_url(version):
  download_urls, name = [], []
  for robo_name, url in _ROBO_URL_FILES.items():
    name.append(robo_name)
    download_urls.append(url)
  partial_manifest = {
      'url': download_urls,
      'name': name,
      'ext': '.jar',
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers()

  latest = sub.add_parser("latest")
  latest.set_defaults(func=lambda _opts: do_latest())

  download = sub.add_parser("get_url")
  download.set_defaults(
      func=lambda _opts: _get_download_url(os.environ['_3PP_VERSION']))

  opts = ap.parse_args()
  opts.func(opts)


if __name__ == '__main__':
  main()

