// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Histogram_h
#define Histogram_h

#include <stdint.h>
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/CurrentTime.h"

namespace base {
class HistogramBase;
};

namespace blink {

class PLATFORM_EXPORT CustomCountHistogram {
 public:
  // Min values should be >=1 as emitted 0s still go into the underflow bucket.
  CustomCountHistogram(const char* name,
                       base::HistogramBase::Sample min,
                       base::HistogramBase::Sample max,
                       int32_t bucket_count);
  void Count(base::HistogramBase::Sample);

 protected:
  explicit CustomCountHistogram(base::HistogramBase*);

  base::HistogramBase* histogram_;
};

class PLATFORM_EXPORT BooleanHistogram : public CustomCountHistogram {
 public:
  BooleanHistogram(const char* name);
};

class PLATFORM_EXPORT EnumerationHistogram : public CustomCountHistogram {
 public:
  // |boundaryValue| must be strictly greater than samples passed to |count|.
  EnumerationHistogram(const char* name,
                       base::HistogramBase::Sample boundary_value);
};

class PLATFORM_EXPORT SparseHistogram {
 public:
  explicit SparseHistogram(const char* name);

  void Sample(base::HistogramBase::Sample);

 private:
  base::HistogramBase* histogram_;
};

class PLATFORM_EXPORT LinearHistogram : public CustomCountHistogram {
 public:
  explicit LinearHistogram(const char* name,
                           base::HistogramBase::Sample min,
                           base::HistogramBase::Sample max,
                           int32_t bucket_count);
};

class PLATFORM_EXPORT ScopedUsHistogramTimer {
 public:
  ScopedUsHistogramTimer(CustomCountHistogram& counter)
      : start_time_(WTF::MonotonicallyIncreasingTime()), counter_(counter) {}

  ~ScopedUsHistogramTimer() {
    counter_.Count((WTF::MonotonicallyIncreasingTime() - start_time_) *
                   base::Time::kMicrosecondsPerSecond);
  }

 private:
  // In seconds.
  double start_time_;
  CustomCountHistogram& counter_;
};

// Use code like this to record time, in microseconds, to execute a block of
// code:
//
// {
//     SCOPED_BLINK_UMA_HISTOGRAM_TIMER(myUmaStatName)
//     RunMyCode();
// }
// This macro records all times between 0us and 10 seconds.
// Do not change this macro without renaming all metrics that use it!
#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER(name)                 \
  DEFINE_STATIC_LOCAL(CustomCountHistogram, scoped_us_counter, \
                      (name, 0, 10000000, 50));                \
  ScopedUsHistogramTimer timer(scoped_us_counter);

}  // namespace blink

#endif  // Histogram_h
