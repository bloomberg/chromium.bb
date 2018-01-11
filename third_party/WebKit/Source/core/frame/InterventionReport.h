// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterventionReport_h
#define InterventionReport_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/MessageReport.h"

namespace blink {

class CORE_EXPORT InterventionReport : public MessageReport {
  DEFINE_WRAPPERTYPEINFO();

 public:
  InterventionReport(const String& message,
                     std::unique_ptr<SourceLocation> location)
      : MessageReport(message, std::move(location)) {}

  ~InterventionReport() override = default;
};

}  // namespace blink

#endif  // InterventionReport_h
