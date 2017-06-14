// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformProbes_h
#define PlatformProbes_h

#include "platform/PlatformExport.h"
#include "platform/loader/fetch/FetchContext.h"

namespace blink {

class FetchContext;
class PlatformProbeSink;

namespace probe {

class PLATFORM_EXPORT ProbeBase {
  STACK_ALLOCATED();

 public:
  double CaptureStartTime() const;
  double CaptureEndTime() const;
  double Duration() const;

 private:
  mutable double start_time_ = 0;
  mutable double end_time_ = 0;
};

inline PlatformProbeSink* ToPlatformProbeSink(FetchContext* context) {
  return context->GetPlatformProbeSink();
}

}  // namespace probe
}  // namespace blink

#include "platform/PlatformProbesInl.h"

#endif  // PlatformProbes_h
