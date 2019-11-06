#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import urllib2

import metrics_utils


def main():
  metrics = raw_input()
  try:
    urllib2.urlopen(metrics_utils.APP_URL + '/upload', metrics)
  except urllib2.HTTPError:
    pass
  except urllib2.URLError:
    pass

  return 0


if __name__ == '__main__':
  sys.exit(main())
