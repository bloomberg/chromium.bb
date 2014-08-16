// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_LEAKY_BUCKET_H_
#define REMOTING_TEST_LEAKY_BUCKET_H_

#include "base/basictypes.h"
#include "base/time/time.h"

namespace remoting {

class LeakyBucket {
 public:
  // |depth| is in bytes. |rate| is specified in bytes/second.
  LeakyBucket(double depth, double rate);
  ~LeakyBucket();

  // Adds a packet of the given |size| to the bucket and returns packet delay.
  // Returns TimeDelta::Max() if the packet overflows the bucket, in which case
  // it should be dropped.
  base::TimeDelta AddPacket(int size);

 private:
  void UpdateLevel();

  double depth_;
  double rate_;

  double level_;
  base::TimeTicks last_update_;

  DISALLOW_COPY_AND_ASSIGN(LeakyBucket);
};

}  // namespace remoting

#endif  // REMOTING_TEST_LEAKY_BUCKET_H_
