// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeprecationReport_h
#define DeprecationReport_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/MessageReport.h"

namespace blink {

class CORE_EXPORT DeprecationReport : public MessageReport {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DeprecationReport(const String& id,
                    double anticipatedRemoval,
                    const String& message,
                    std::unique_ptr<SourceLocation> location)
      : MessageReport(message, std::move(location)),
        id_(id),
        anticipatedRemoval_(anticipatedRemoval) {}

  ~DeprecationReport() override = default;

  String id() const { return id_; }
  double anticipatedRemoval(bool& is_null) const {
    is_null = !anticipatedRemoval_;
    return anticipatedRemoval_;
  }

 private:
  const String id_;
  const double anticipatedRemoval_;
};

}  // namespace blink

#endif  // DeprecationReport_h
