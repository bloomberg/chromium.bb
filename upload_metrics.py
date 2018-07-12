#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import urllib2


APP_URL = 'https://cit-cli-metrics.appspot.com'


def main():
  metrics = raw_input()
  try:
    urllib2.urlopen(APP_URL + '/upload', metrics)
  except urllib2.HTTPError:
    pass

  return 0


if __name__ == '__main__':
  sys.exit(main())
