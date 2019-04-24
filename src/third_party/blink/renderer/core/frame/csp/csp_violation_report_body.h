// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_VIOLATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_VIOLATION_REPORT_BODY_H_

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/events/security_policy_violation_event_init.h"
#include "third_party/blink/renderer/core/frame/report_body.h"

namespace blink {

class CORE_EXPORT CSPViolationReportBody : public ReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSPViolationReportBody* Create(
      const SecurityPolicyViolationEventInit& violation_data) {
    return MakeGarbageCollected<CSPViolationReportBody>(violation_data);
  }

  CSPViolationReportBody(const SecurityPolicyViolationEventInit& violation_data)
      : document_url_(violation_data.documentURI()),
        referrer_(violation_data.referrer()),
        blocked_url_(violation_data.blockedURI()),
        effective_directive_(violation_data.effectiveDirective()),
        original_policy_(violation_data.originalPolicy()),
        source_file_(violation_data.sourceFile()),
        sample_(violation_data.sample()),
        disposition_(violation_data.disposition()),
        status_code_(violation_data.statusCode()),
        line_number_(source_file_ ? violation_data.lineNumber() : 0),
        column_number_(source_file_ ? violation_data.columnNumber() : 0) {}

  ~CSPViolationReportBody() override = default;

  String documentURL() const { return document_url_; }
  String referrer() const { return referrer_; }
  String blockedURL() const { return blocked_url_; }
  String effectiveDirective() const { return effective_directive_; }
  String originalPolicy() const { return original_policy_; }
  String sourceFile() const { return source_file_; }
  String sample() const { return sample_; }
  String disposition() const { return disposition_; }
  uint16_t statusCode() const { return status_code_; }

  uint32_t lineNumber(bool& is_null) const {
    is_null = !source_file_;
    return line_number_;
  }

  uint32_t columnNumber(bool& is_null) const {
    is_null = !source_file_;
    return column_number_;
  }

 private:
  const String document_url_;
  const String referrer_;
  const String blocked_url_;
  const String effective_directive_;
  const String original_policy_;
  const String source_file_;
  const String sample_;
  const String disposition_;
  const uint16_t status_code_;
  const uint32_t line_number_;
  const uint32_t column_number_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_VIOLATION_REPORT_BODY_H_
