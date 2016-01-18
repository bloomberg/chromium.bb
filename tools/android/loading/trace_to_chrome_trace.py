#! /usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert trace output for Chrome.

Take the tracing track output from tracing_driver.py to a zip'd json that can be
loading by chrome devtools tracing.
"""

import argparse
import gzip
import json

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('input')
  parser.add_argument('output')
  args = parser.parse_args()
  with gzip.GzipFile(args.output, 'w') as output_f, file(args.input) as input_f:
    events = json.load(input_f)
    json.dump({'traceEvents': events, 'metadata': {}}, output_f)
