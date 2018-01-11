// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MessageReport_h
#define MessageReport_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/ReportBody.h"

namespace blink {

class MessageReport : public ReportBody {
 public:
  MessageReport(const String& message, std::unique_ptr<SourceLocation> location)
      : message_(message), location_(std::move(location)) {}

  ~MessageReport() override = default;

  String message() const { return message_; }
  long lineNumber() const { return location_->LineNumber(); }
  long columnNumber() const { return location_->ColumnNumber(); }
  String sourceFile() const {
    return location_->Url().IsNull() ? "" : location_->Url();
  }

 protected:
  const String message_;
  std::unique_ptr<SourceLocation> location_;
};

}  // namespace blink

#endif  // MessageReport_h
