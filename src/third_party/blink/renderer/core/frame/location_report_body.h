// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/frame/report_body.h"

namespace blink {

class LocationReportBody : public ReportBody {
 public:
  explicit LocationReportBody(std::unique_ptr<SourceLocation> location)
      : location_(std::move(location)) {}

  LocationReportBody() : LocationReportBody(SourceLocation::Capture()) {}

  LocationReportBody(const String& source_file,
                     unsigned line_number,
                     unsigned column_number)
      : LocationReportBody(
            SourceLocation::Capture(source_file, line_number, column_number)) {}

  ~LocationReportBody() override = default;

  String sourceFile() const { return location_->Url(); }

  uint32_t lineNumber(bool& is_null) const {
    is_null = location_->IsUnknown();
    return location_->LineNumber();
  }

  uint32_t columnNumber(bool& is_null) const {
    is_null = location_->IsUnknown();
    return location_->ColumnNumber();
  }

 protected:
  std::unique_ptr<SourceLocation> location_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_
