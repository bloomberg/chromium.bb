// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_MESSAGE_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_MESSAGE_REPORT_BODY_H_

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/frame/location_report_body.h"

namespace blink {

class MessageReportBody : public LocationReportBody {
 public:
  MessageReportBody(const String& message)
      : LocationReportBody(), message_(message) {}

  ~MessageReportBody() override = default;

  String message() const { return message_; }

 protected:
  const String message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_MESSAGE_REPORT_BODY_H_
