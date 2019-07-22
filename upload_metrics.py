#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from third_party.six.moves import urllib
from third_party.six.moves import input # pylint: disable=redefined-builtin

import metrics_utils


def main():
  metrics = input()
  try:
    urllib.request.urlopen(
        metrics_utils.APP_URL + '/upload', metrics.encode('utf-8'))
  except (urllib.error.HTTPError, urllib.error.URLError):
    pass

  return 0


if __name__ == '__main__':
  sys.exit(main())
