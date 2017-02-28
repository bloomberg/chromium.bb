// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SecurityViolationReportingPolicy_h
#define SecurityViolationReportingPolicy_h

namespace blink {

enum class SecurityViolationReportingPolicy {
  SuppressReporting,
  Report,
};

}  // namespace blink

#endif  // SecurityViolationReportingPolicy_h
