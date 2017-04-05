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
  STACK_ALLOCATED()

 public:
  double captureStartTime() const;
  double captureEndTime() const;
  double duration() const;

 private:
  mutable double m_startTime = 0;
  mutable double m_endTime = 0;
};

inline PlatformProbeSink* toPlatformProbeSink(FetchContext* context) {
  return context->platformProbeSink();
}

}  // namespace probe
}  // namespace blink

#include "platform/PlatformProbesInl.h"

#endif  // PlatformProbes_h
