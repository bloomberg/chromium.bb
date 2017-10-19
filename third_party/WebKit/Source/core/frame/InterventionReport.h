// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterventionReport_h
#define InterventionReport_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/ReportBody.h"

namespace blink {

// TODO(paulmeyer): Consider refactor with DeprecationReport.h.
class CORE_EXPORT InterventionReport : public ReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  InterventionReport(const String& message,
                     std::unique_ptr<SourceLocation> location)
      : message_(message), location_(std::move(location)) {}

  ~InterventionReport() override {}

  String message() const { return message_; }
  long lineNumber() const { return location_->LineNumber(); }
  String sourceFile() const {
    return location_->Url().IsNull() ? "" : location_->Url();
  }

  virtual void Trace(blink::Visitor* visitor) { ReportBody::Trace(visitor); }

 private:
  const String message_;
  std::unique_ptr<SourceLocation> location_;
};

}  // namespace blink

#endif  // InterventionReport_h
