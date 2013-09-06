# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import bisect
import math

def Clamp(value, low=0.0, high=1.0):
  return min(max(value, low), high)

def NormalizeSamples(samples):
  ''' Sort the N samples, and map them linearly to the range [0,1] such that the
      first sample is 0.5/N and the last sample is (N-0.5)/N. Background: the
      discrepancy of the sample set i/(N-1); i=0,...,N-1 is 2/N, twice the
      discrepancy of the sample set (i+1/2)/N; i=0,...,N-1. In our case we
      don't want to distinguish between these two cases, as our original domain
      is not bounded (it is for Monte Carlo integration, where discrepancy was
      first used).
  '''
  samples = sorted(samples)
  low = min(samples)
  high = max(samples)
  new_low = 0.5 / len(samples)
  new_high = (len(samples)-0.5) / len(samples)
  scale = (new_high - new_low) / (high - low)
  for i in xrange(0, len(samples)):
    samples[i] = float(samples[i] - low) * scale + new_low
  return samples, scale

def Discrepancy(samples, interval_multiplier = 10000):
  ''' Compute the discrepancy of a set of 1D samples from the unit interval
      [0,1]. The samples must be sorted.

      http://en.wikipedia.org/wiki/Low-discrepancy_sequence
      http://mathworld.wolfram.com/Discrepancy.html
  '''
  if (len(samples) < 3):
    return 0

  max_local_discrepancy = 0
  locations = []
  # For each location, stores the number of samples less than that location.
  left = []
  # For each location, stores the number of samples less than or equal to that
  # location.
  right = []

  interval_count = len(samples) * interval_multiplier
  # Compute number of locations the will roughly result in the requested number
  # of intervals.
  location_count = int(math.ceil(math.sqrt(interval_count*2)))
  inv_sample_count = 1.0 / len(samples)

  # Generate list of equally spaced locations.
  for i in xrange(0, location_count):
    location = float(i) / (location_count-1)
    locations.append(location)
    left.append(bisect.bisect_left(samples, location))
    right.append(bisect.bisect_right(samples, location))

  # Iterate over the intervals defined by any pair of locations.
  for i in xrange(0, len(locations)):
    for j in xrange(i, len(locations)):
      # Compute length of interval and number of samples in the interval.
      length = locations[j] - locations[i]
      count = right[j] - left[i]

      # Compute local discrepancy and update max_local_discrepancy.
      local_discrepancy = abs(float(count)*inv_sample_count - length)
      max_local_discrepancy = max(local_discrepancy, max_local_discrepancy)

  return max_local_discrepancy

def FrameDiscrepancy(frame_timestamps, absolute = True,
                     interval_multiplier = 10000):
  ''' A discrepancy based metric for measuring jank.

      FrameDiscrepancy quantifies the largest area of jank observed in a series
      of timestamps.  Note that this is different form metrics based on the
      max_frame_time. For example, the time stamp series A = [0,1,2,3,5,6] and
      B = [0,1,2,3,5,7] have the same max_frame_time = 2, but
      Discrepancy(B) > Discrepancy(A).

      Two variants of discrepancy can be computed:

      Relative discrepancy is following the original definition of
      discrepancy. It characterized the largest area of jank, relative to the
      duration of the entire time stamp series.  We normalize the raw results,
      because the best case discrepancy for a set of N samples is 1/N (for
      equally spaced samples), and we want our metric to report 0.0 in that
      case.

      Absolute discrepancy also characterizes the largest area of jank, but its
      value wouldn't change (except for imprecisions due to a low
      interval_multiplier) if additional 'good' frames were added to an
      exisiting list of time stamps.  Its range is [0,inf] and the unit is
      milliseconds.

      The time stamp series C = [0,2,3,4] and D = [0,2,3,4,5] have the same
      absolute discrepancy, but D has lower relative discrepancy than C.
  '''
  samples, sample_scale = NormalizeSamples(frame_timestamps)
  discrepancy = Discrepancy(samples, interval_multiplier)
  inv_sample_count = 1.0 / len(samples)
  if absolute:
    # Compute absolute discrepancy
    discrepancy /= sample_scale
  else:
    # Compute relative discrepancy
    discrepancy = Clamp((discrepancy-inv_sample_count) / (1.0-inv_sample_count))
  return discrepancy
