// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorKeyframe_h
#define CompositorKeyframe_h

#include "platform/PlatformExport.h"
#include "platform/wtf/RefPtr.h"

namespace cc {
class TimingFunction;
}

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorKeyframe {
 public:
  virtual ~CompositorKeyframe() {}

  virtual double Time() const = 0;

  scoped_refptr<TimingFunction> GetTimingFunctionForTesting() const;

 private:
  virtual const cc::TimingFunction* CcTimingFunction() const = 0;
};

}  // namespace blink

#endif  // CompositorKeyframe_h
