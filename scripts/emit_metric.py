# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The build API Metrics Emit entry point."""

from __future__ import print_function

from chromite.lib import commandline
from chromite.utils import metrics


def main(argv):
  """Emit a metric event."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('op', choices=metrics.VALID_OPS,
                      help='Which metric event operator to emit.')
  parser.add_argument('name',
                      help='The name of the metric event as you would like it '
                           'to appear downstream in data stores.')
  parser.add_argument('key', nargs='?',
                      help='A unique key for this invocation to ensure that '
                           'start and stop timers can be matched.')
  opts = parser.parse_args(argv)

  timestamp = metrics.current_milli_time()
  key = opts.key or opts.name
  metrics.append_metrics_log(timestamp, opts.name, opts.op, key=key)
