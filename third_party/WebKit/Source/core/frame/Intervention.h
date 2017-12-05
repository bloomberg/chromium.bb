// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Intervention_h
#define Intervention_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Intervention {
  DISALLOW_NEW();

 public:
  Intervention() = default;
  ~Intervention() = default;

  // Generates a intervention report, to be routed to the Reporting API and any
  // ReportingObservers. Also sends the intervention message to the console.
  static void GenerateReport(const LocalFrame*, const String& message);

  DISALLOW_COPY_AND_ASSIGN(Intervention);
};

}  // namespace blink

#endif  // Intervention_h
