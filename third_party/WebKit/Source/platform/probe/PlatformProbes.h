// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformProbes_h
#define PlatformProbes_h

#include "platform/PlatformExport.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/wtf/Time.h"

namespace blink {

class FetchContext;
class PlatformProbeSink;

namespace probe {

class PLATFORM_EXPORT ProbeBase {
  STACK_ALLOCATED();

 public:
  TimeTicks CaptureStartTime() const;
  TimeTicks CaptureEndTime() const;
  TimeDelta Duration() const;

 private:
  mutable TimeTicks start_time_;
  mutable TimeTicks end_time_;
};

inline PlatformProbeSink* ToPlatformProbeSink(FetchContext* context) {
  return context->GetPlatformProbeSink();
}

}  // namespace probe
}  // namespace blink

#include "platform/PlatformProbesInl.h"

#endif  // PlatformProbes_h
