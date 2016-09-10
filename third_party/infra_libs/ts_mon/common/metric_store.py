# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import threading
import time

from infra_libs.ts_mon.common import errors


"""A light-weight representation of a set or an incr.

Args:
  name: The metric name.
  fields: The normalized field tuple.
  mod_type: Either 'set' or 'incr'.  Other values will raise
            UnknownModificationTypeError when it's used.
  args: (value, enforce_ge) for 'set' or (delta, modify_fn) for 'incr'.
"""  # pylint: disable=pointless-string-statement
Modification = collections.namedtuple(
    'Modification', ['name', 'fields', 'mod_type', 'args'])


def default_modify_fn(name):
  def _modify_fn(value, delta):
    if delta < 0:
      raise errors.MonitoringDecreasingValueError(name, None, delta)
    return value + delta
  return _modify_fn


class MetricStore(object):
  """A place to store values for each metric.

  Several methods take "a normalized field tuple".  This is a tuple of
  (key, value) tuples sorted by key.  (The reason this is given as a tuple
  instead of a dict is because tuples are hashable and can be used as dict keys,
  dicts can not).

  The MetricStore is also responsible for keeping the start_time of each metric.
  This is what goes into the start_timestamp_us field in the MetricsData proto
  for cumulative metrics and distributions, and helps Monarch identify when a
  counter was reset.  This is the MetricStore's job because an implementation
  might share counter values across multiple instances of a task (like on
  Appengine), so the start time must be associated with that value so that it
  can be reset for all tasks at once when the value is reset.

  External metric stores (like those backed by memcache) may be cleared (either
  wholly or partially) at any time.  When this happens the MetricStore *must*
  generate a new start_time for all the affected metrics.

  Metrics can specify their own explicit start time if they are mirroring the
  value of some external counter that started counting at a known time.

  Otherwise the MetricStore's time_fn (defaults to time.time()) is called the
  first time a metric is set or incremented, or after it is cleared externally.
  """

  def __init__(self, state, time_fn=None):
    self._state = state
    self._time_fn = time_fn or time.time

  def get(self, name, fields, default=None):
    """Fetches the current value for the metric.

    Args:
      name: the metric's name.
      fields: a normalized field tuple.
      default: the value to return if the metric has no value of this set of
          field values.
    """
    raise NotImplementedError

  def get_all(self):
    """Returns an iterator over all the metrics present in the store.

    The iterator yields 4-tuples:
      (target, metric, start_time, field_values)
    """
    raise NotImplementedError

  def set(self, name, fields, value, enforce_ge=False):
    """Sets the metric's value.

    Args:
      name: the metric's name.
      fields: a normalized field tuple.
      value: the new value for the metric.
      enforce_ge: if this is True, raise an exception if the new value is
          less than the old value.

    Raises:
      MonitoringDecreasingValueError: if enforce_ge is True and the new value is
          smaller than the old value.
    """
    raise NotImplementedError

  def incr(self, name, fields, delta, modify_fn=None):
    """Increments the metric's value.

    Args:
      name: the metric's name.
      fields: a normalized field tuple.
      delta: how much to increment the value by.
      modify_fn: this function is called with the original value and the delta
          as its arguments and is expected to return the new value.  The
          function must be idempotent as it may be called multiple times.
    """
    raise NotImplementedError

  def modify_multi(self, modifications):
    """Modifies multiple metrics in one go.

    Args:
      modifications: an iterable of Modification objects.
    """
    raise NotImplementedError

  def reset_for_unittest(self, name=None):
    """Clears the values metrics.  Useful in unittests.

    Args:
      name: the name of an individual metric to reset, or if None resets all
        metrics.
    """
    raise NotImplementedError

  def initialize_context(self):
    """Opens a request-local context for deferring metric updates."""
    pass  # pragma: no cover

  def finalize_context(self):
    """Closes a request-local context opened by initialize_context."""
    pass  # pragma: no cover

  def _start_time(self, name):
    if name in self._state.metrics:
      ret = self._state.metrics[name].start_time
      if ret is not None:
        return ret

    return self._time_fn()


class InProcessMetricStore(MetricStore):
  """A thread-safe metric store that keeps values in memory."""

  def __init__(self, state, time_fn=None):
    super(InProcessMetricStore, self).__init__(state, time_fn=time_fn)

    self._values = {}
    self._thread_lock = threading.Lock()

  def _entry(self, name):
    if name not in self._values:
      self._reset(name)

    return self._values[name]

  def get(self, name, fields, default=None):
    return self._entry(name)[1].get(fields, default)

  def get_all(self):
    state = self._state
    for name, (start_time, fields_values) in self._values.iteritems():
      if name in state.metrics:  # pragma: no cover
        yield state.target, state.metrics[name], start_time, fields_values

  def set(self, name, fields, value, enforce_ge=False):
    with self._thread_lock:
      if enforce_ge:
        old_value = self._entry(name)[1].get(fields, 0)
        if value < old_value:
          raise errors.MonitoringDecreasingValueError(name, old_value, value)

      self._entry(name)[1][fields] = value

  def incr(self, name, fields, delta, modify_fn=None):
    if delta < 0:
      raise errors.MonitoringDecreasingValueError(name, None, delta)

    if modify_fn is None:
      modify_fn = default_modify_fn(name)

    with self._thread_lock:
      self._entry(name)[1][fields] = modify_fn(self.get(name, fields, 0), delta)

  def modify_multi(self, modifications):
    # This is only used by DeferredMetricStore on top of MemcacheMetricStore,
    # but could be implemented here if required in the future.
    raise NotImplementedError

  def reset_for_unittest(self, name=None):
    if name is not None:
      self._reset(name)
    else:
      for name in self._values.keys():
        self._reset(name)

  def _reset(self, name):
    self._values[name] = (self._start_time(name), {})
