// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Intervention_h
#define Intervention_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Intervention {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(Intervention);

 public:
  Intervention() {}
  ~Intervention() {}

  // Generates an intervention report, and sends the intervention message to the
  // console. The report will be routed to any ReportingObservers.
  static void GenerateReport(const LocalFrame*, const String& message);
};

}  // namespace blink

#endif  // Intervention_h
