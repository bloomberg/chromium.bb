// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTriggeringEventInfo_h
#define WebTriggeringEventInfo_h

namespace blink {

// Extra info sometimes associated with a navigation. Mirrors
// theWebTriggeringEventInfoEnum.
enum class WebTriggeringEventInfo {
  kUnknown,

  // The navigation was not triggered via a JS Event.
  kNotFromEvent,

  // The navigation was triggered via a JS event with isTrusted() == true.
  kFromTrustedEvent,

  // The navigation was triggered via a JS event with isTrusted() == false.
  kFromUntrustedEvent,

  kLast,
};

}  // namespace blink

#endif  // WebTriggeringEventInfo_h
