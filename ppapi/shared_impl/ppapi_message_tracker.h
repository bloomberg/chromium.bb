// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_MESSAGE_TRACKER_H_
#define PPAPI_SHARED_IMPL_PPAPI_MESSAGE_TRACKER_H_

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

template <typename T> struct DefaultSingletonTraits;

namespace ppapi {

// PpapiMessageTracker uses a counter to record whether anyone is sending or
// receiving pepper messages in the current process.
// This class is thread safe.
class PPAPI_SHARED_EXPORT PpapiMessageTracker {
 public:
  static PpapiMessageTracker* GetInstance();

  void EnterMessageHandling();
  void ExitMessageHandling();
  bool IsHandlingMessage();

 private:
  friend struct DefaultSingletonTraits<PpapiMessageTracker>;

  PpapiMessageTracker();
  ~PpapiMessageTracker();

  base::Lock lock_;
  int enter_count_;

  DISALLOW_COPY_AND_ASSIGN(PpapiMessageTracker);
};

class PPAPI_SHARED_EXPORT ScopedTrackPpapiMessage {
 public:
  ScopedTrackPpapiMessage();
  ~ScopedTrackPpapiMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTrackPpapiMessage);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_MESSAGE_TRACKER_H_
