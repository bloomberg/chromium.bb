// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleTypes_h
#define ConsoleTypes_h

namespace blink {

enum MessageSource {
  kXMLMessageSource,
  kJSMessageSource,
  kNetworkMessageSource,
  kConsoleAPIMessageSource,
  kStorageMessageSource,
  kAppCacheMessageSource,
  kRenderingMessageSource,
  kSecurityMessageSource,
  kOtherMessageSource,
  kDeprecationMessageSource,
  kWorkerMessageSource,
  kViolationMessageSource,
  kInterventionMessageSource,
  kRecommendationMessageSource
};

enum MessageLevel {
  kVerboseMessageLevel,
  kInfoMessageLevel,
  kWarningMessageLevel,
  kErrorMessageLevel
};
}

#endif  // !defined(ConsoleTypes_h)
