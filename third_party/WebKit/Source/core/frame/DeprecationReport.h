// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeprecationReport_h
#define DeprecationReport_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/ReportBody.h"

namespace blink {

class CORE_EXPORT DeprecationReport : public ReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DeprecationReport(const String& message,
                    std::unique_ptr<SourceLocation> location)
      : message_(message), location_(std::move(location)) {}

  ~DeprecationReport() override {}

  String message() const { return message_; }
  String sourceFile() const { return location_->Url(); }
  long lineNumber() const { return location_->LineNumber(); }

  DEFINE_INLINE_VIRTUAL_TRACE() { ReportBody::Trace(visitor); }

 private:
  const String message_;
  std::unique_ptr<SourceLocation> location_;
};

}  // namespace blink

#endif  // DeprecationReport_h
