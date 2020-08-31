// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/location_report_body.h"

namespace blink {

// static
LocationReportBody::ReportLocation LocationReportBody::CreateReportLocation(
    const String& file,
    base::Optional<uint32_t> line_number,
    base::Optional<uint32_t> column_number) {
  return file.IsEmpty() ? CreateReportLocation(SourceLocation::Capture())
                        : ReportLocation{file, line_number, column_number};
}

// static
LocationReportBody::ReportLocation LocationReportBody::CreateReportLocation(
    std::unique_ptr<SourceLocation> location) {
  return location->IsUnknown()
             ? ReportLocation{}
             : ReportLocation{location->Url(), location->LineNumber(),
                              location->ColumnNumber()};
}

void LocationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddStringOrNull("sourceFile", sourceFile());
  if (lineNumber()) {
    builder.AddNumber("lineNumber", lineNumber().value());
  } else {
    builder.AddNull("lineNumber");
  }
  if (columnNumber()) {
    builder.AddNumber("columnNumber", columnNumber().value());
  } else {
    builder.AddNull("columnNumber");
  }
}

}  // namespace blink
