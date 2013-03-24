// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class chooses a capture interval so as to limit CPU usage to not exceed
// a specified %age. It bases this on the CPU usage of recent capture and encode
// operations, and on the number of available CPUs.

#ifndef REMOTING_HOST_CAPTURE_SCHEDULER_H_
#define REMOTING_HOST_CAPTURE_SCHEDULER_H_

#include "base/time.h"
#include "remoting/base/running_average.h"

namespace remoting {

class CaptureScheduler {
 public:
  CaptureScheduler();
  ~CaptureScheduler();

  // Returns the time to wait after initiating a capture before triggering
  // the next.
  base::TimeDelta NextCaptureDelay();

  // Records time spent on capturing and encoding.
  void RecordCaptureTime(base::TimeDelta capture_time);
  void RecordEncodeTime(base::TimeDelta encode_time);

  // Overrides the number of processors for testing.
  void SetNumOfProcessorsForTest(int num_of_processors);

 private:
  int num_of_processors_;
  RunningAverage capture_time_;
  RunningAverage encode_time_;

  DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURE_SCHEDULER_H_
