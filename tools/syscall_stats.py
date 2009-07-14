#!/usr/bin/python
#
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



"""
Filter service runtime logging output and compute system call statistics.

To use this script, define the BENCHMARK symbol to be zero (default)
in nacl_syscall_hook.c.  Next, run the service runtime with NACLLOG
set to an output file name.  When the run is complete, run this script
with that file as input.

"""

import math
import re
import sys


class Stats:
  """
  Compute basic statistics.
  """
  def __init__(self):
    self._sum_x = 0.0
    self._sum_x_squared = 0.0
    self._n = 0
  # enddef

  def Enter(self, val):
    """Enter a new value.

    Args:
      val: the new (floating point) value
    """
    self._sum_x += val
    self._sum_x_squared += val * val
    self._n += 1
  # enddef

  def Mean(self):
    """Returns the mean of entered values.
    """
    return self._sum_x / self._n
  # enddef

  def Variance(self):
    """Returns the variance of entered values.
    """
    mean = self.Mean()
    return self._sum_x_squared / self._n - mean * mean
  # enddef

  def Stddev(self):
    """Returns the standard deviation of entered values.
    """
    return math.sqrt(self.Variance())
  # enddef

  def NumEntries(self):
    """Returns the number of data points entered.
    """
    return self._n
  # enddef
# endclass


class PeakStats:
  """Compute min and max for a data set.  While far less efficient
  than using a reduce, this class makes streaming data handling
  easier.
  """

  def __init__(self):
    self._min = 1L << 64
    self._max = -1
  # enddef

  def Enter(self, val):
    """Enter a new datum.

    Args:
      val: the new datum to be entered.
    """
    if val > self._max:
      self._max = val
    # endif
    if val < self._min:
      self._min = val
    # endif
  # enddef

  def Max(self):
    """Returns the maximum value found so far.
    """
    return self._max
  # enddef

  def Min(self):
    """Returns the minimum value found so far.
    """
    return self._min
  # enddef
# endclass


class WindowedRate:

  """Class for computing statistics on events based on counting the
  number of occurrences in a time interval.  Statistcs on these
  bucketed counts are then available.

  """
  def __init__(self, duration):
    self._t_start = -1
    self._t_duration = duration
    self._t_end = -1
    self._event_count = 0
    self._rate_stats = Stats()
    self._peak_stats = PeakStats()
  # enddef

  def Enter(self, t):
    """Enter in a new event that occurred at time t.

    Args:
      t: the time at which an event occurred.
    """
    if self._t_start == -1:
      self._t_start = t
      self._t_end = t + self._t_duration
      return
    # [ t_start, t_start + duration )
    if t < self._t_end:
      self._event_count += 1
      return
    # endif
    self.Compute()
    self._event_count = 1
    next_end = self._t_end
    while next_end < t:
      next_end += self._t_duration
    # endwhile
    self._t_end = next_end
    self._t_start = next_end - self._t_duration
  # enddef

  def Compute(self):
    """Finalize the last bucket.

    """
    self._rate_stats.Enter(self._event_count)
    self._peak_stats.Enter(self._event_count)
    self._event_count = 0
  # enddef

  def RateStats(self):
    """Returns the event rate statistics object.

    """
    return self._rate_stats
  # enddef

  def PeakStats(self):
    """Returns the peak event rate statistics object.

    """
    return self._peak_stats
  # endif
# endclass


class TimestampParser:
  """
  A class to parse timestamp strings.  This is needed because there is
  implicit state: the timestamp string is HH:MM:SS.fract and may cross
  a 24 hour boundary -- we do not log the date since that would make
  the log file much larger and generally it is not needed (implicit in
  file modification time) -- so we convert to a numeric representation
  that is relative to an arbitrary epoch start, and the state enables
  us to correctly handle midnight.

  This code assumes that the timestamps are monotonically
  non-decreasing.

  """
  def __init__(self):
    self._min_time = -1
  # enddef

  def Convert(self, timestamp):
    """Converts a timestamp string into a numeric timestamp value.

    Args:
      timestamp: A timestamp string in HH:MM:SS.fraction format.

    Returns:
      a numeric timestamp (arbitrary epoch)
    """
    (hh, mm, ss) = map(float,timestamp.split(':'))
    t = ((hh * 60) + mm) * 60 + ss
    if self._min_time == -1:
      self._min_time = t
    # endif
    while t < self._min_time:
      t += 24 * 60 * 60
    # endwhile
    self._min_time = t
    return t
  # enddef
# endclass


def ReadFileHandle(fh, duration):
  """Reads log data from the provided file handle, and compute and
  print various statistics on the system call rate based on the log
  data.

  """
  # log format "[pid:timestamp] msg" where the timestamp is
  log_re = re.compile(r'\[[0-9,]+:([:.0-9]+)\] system call [0-9]+')
  parser = TimestampParser()
  inter_stats = Stats()
  rate_stats = Stats()
  windowed = WindowedRate(duration)
  prev_time = -1
  start_time = 0
  for line in fh:  # generator
    m = log_re.search(line)
    if m is not None:
      timestamp = m.group(1)
      t = parser.Convert(timestamp)

      windowed.Enter(t)

      if prev_time != -1:
        elapsed = t - prev_time
        inter_stats.Enter(elapsed)
        rate_stats.Enter(1.0/elapsed)
      else:
        start_time = t
      # endif
      prev_time = t

    # endif
  # endfor

  print '\nInter-syscall time'
  print 'Mean:   %g' % inter_stats.Mean()
  print 'Stddev: %g' % inter_stats.Stddev()
  print '\nInstantaneous Syscall Rate (unweighted!)'
  print 'Mean :  %g' % rate_stats.Mean()
  print 'Stddev: %g' % rate_stats.Stddev()
  print '\nAvg Syscall Rate: %g' % (rate_stats.NumEntries()
                                    / (prev_time - start_time))

  print '\nSyscalls in %f interval' % duration
  print 'Mean:   %g' % windowed.RateStats().Mean()
  print 'Stddev: %g' % windowed.RateStats().Stddev()
  print 'Min:    %g' % windowed.PeakStats().Min()
  print 'Max:    %g' % windowed.PeakStats().Max()
# enddef


def main(argv):
  if len(argv) > 1:
    print >>sys.stderr, 'no arguments expected\n'
    return 1
  # endif
  ReadFileHandle(sys.stdin, 0.010)
  return 0
# enddef

if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
