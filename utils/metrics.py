# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Metrics for general consumption.

See infra/proto/metrics.proto for a description of the type of record that this
module will be creating.
"""

from __future__ import print_function

import collections
import contextlib
import functools
import os
import tempfile
import time
import uuid

from chromite.lib import cros_logging as logging
from chromite.lib import locking

UTILS_METRICS_LOG_ENVVAR = 'BUILD_API_METRICS_LOG'

OP_START_TIMER = 'start-timer'
OP_STOP_TIMER = 'stop-timer'
OP_NAMED_EVENT = 'event'
VALID_OPS = (OP_START_TIMER, OP_STOP_TIMER, OP_NAMED_EVENT)

# MetricEvent store a start or a stop to a timer. Timers are keyed
# with a unique value to make matching the bookends easier.
MetricEvent = collections.namedtuple('MetricEvent', ('timestamp_epoch_millis',
                                                     'name', 'op', 'key'))


class Error(Exception):
  """Base Error class for other Error types to derive from."""


class ParseMetricError(Error):
  """ParseMetricError represents a coding error in metric events.

  If you see this error there is probably an error in your metric event
  emission code.
  """


def current_milli_time():
  """Return the current Epoch time in milliseconds."""
  return int(round(time.time() * 1000))


def parse_timer(terms):
  """Parse a timer line.

  Args:
    terms: A list of the subdimensions of the MetricEvent type.

  Returns:
    A MetricEvent from the content of the terms.

  Raises:
    ParseMetricError: An error occurred parsing the data from the list of terms.
  """
  if len(terms) != 4:
    raise ParseMetricError('Incorrect number of terms for timer metric. Should '
                           'have been 4, instead it is %d. See terms %s.' %
                           (len(terms), terms))

  assert terms[2] in {OP_START_TIMER, OP_STOP_TIMER}
  return MetricEvent(int(terms[0]), terms[1], terms[2], terms[3])


def parse_named_event(terms):
  """Parse a named event line.

  Args:
    terms: A list of the subdimensions of the MetricEvent type, omitting "key".

  Returns:
    A MetricEvent from the content of the terms.

  Raises:
    ParseMetricError: An error occurred parsing the data from the list of terms.
  """
  if len(terms) != 3:
    raise ParseMetricError('Incorrect number of terms for event metric. Should '
                           'have been 3, instead it is %d. See terms %s.' %
                           (len(terms), terms))

  assert terms[2] == OP_NAMED_EVENT
  return MetricEvent(int(terms[0]), terms[1], terms[2], key=None)


def get_metric_parser(op):
  """Return a function which can parse a line with this operator."""
  return {
      OP_START_TIMER: parse_timer,
      OP_STOP_TIMER: parse_timer,
      OP_NAMED_EVENT: parse_named_event,
  }[op]


def parse_metric(line):
  """Take a line and return a MetricEvent."""
  terms = line.strip().split('|')
  if 3 <= len(terms) <= 4:
    # Get a parser for this (call the factory).
    parser = get_metric_parser(terms[2])
    if parser:
      return parser(terms)

  raise ParseMetricError('Malformed metric line: %s' % line)


def read_metrics_events():
  """Generate metric events by parsing the metrics log file."""
  metrics_logfile = os.environ.get(UTILS_METRICS_LOG_ENVVAR)
  if not metrics_logfile:
    return

  logging.info('reading metrics logs from %s', metrics_logfile)
  # TODO(wbbradley): Drop this once it's stable https://crbug.com/1001909.
  with open(metrics_logfile) as f:
    logging.info('[metrics log file]\n%s', f.read())
  with open(metrics_logfile, 'r') as f:
    for line in f:
      yield parse_metric(line)


def collect_metrics(functor):
  """Enable metric collection by setting up a temp file and env var."""
  @functools.wraps(functor)
  def wrapper(*args, **kwargs):
    """Wrapped function which implements collect_metrics behavior."""
    metrics_logfile = os.environ.get(UTILS_METRICS_LOG_ENVVAR)
    if metrics_logfile:
      # We are in a reentrant scenario, let's just pass the logfile name along.
      return functor(*args, **kwargs)
    else:
      # Let's manage the lifetime of a logfile for consumption within functor.
      tmp_prefix = 'build-metrics-'
      with tempfile.NamedTemporaryFile(prefix=tmp_prefix) as temp_file:
        os.environ[UTILS_METRICS_LOG_ENVVAR] = temp_file.name
        logging.info('Setting up metrics collection (%s=%s).',
                     UTILS_METRICS_LOG_ENVVAR, temp_file.name)
        try:
          return functor(*args, **kwargs)
        finally:
          del os.environ[UTILS_METRICS_LOG_ENVVAR]
  return wrapper


def append_metrics_log(timestamp, name, op, key=None):
  """Handle appending a list of terms to the metrics log.

  If the environment does not specify a metrics log, then skip silently.

  Args:
    timestamp: A millisecond epoch timestamp.
    name: A period-separated string describing the event.
    op: One of the OP_* values, determining which type of event this is.
    key: An optional key to disambiguate equivalenty named events.
  """
  metrics_log = os.environ.get(UTILS_METRICS_LOG_ENVVAR)
  terms = [timestamp, name.replace('|', '_'), op]
  if key:
    terms.append(key)

  # Format the actual line to log.
  line = '|'.join(str(x) for x in terms)
  if metrics_log:
    with locking.FileLock(metrics_log).write_lock():
      with open(metrics_log, 'a') as f:
        f.write('%s\n' % line)


@contextlib.contextmanager
def timer(name):
  """A context manager to emit start/stop events.

  Args:
    name: A name for the timer event.

  Yields:
    Context for context manager surrounding event emission.
  """
  # Timer events use a "key" to disambiguate in case of multiple concurrent or
  # overlapping timers with the same name.
  key = uuid.uuid4()
  try:
    append_metrics_log(current_milli_time(), name, OP_START_TIMER, key=key)
    yield
  finally:
    append_metrics_log(current_milli_time(), name, OP_STOP_TIMER, key=key)


def event(name):
  """Emit a counter event.

  Args:
    name: A name for the timer event.
  """
  append_metrics_log(current_milli_time(), name, OP_NAMED_EVENT)
