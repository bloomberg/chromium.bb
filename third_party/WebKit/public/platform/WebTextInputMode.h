// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTextInputMode_h
#define WebTextInputMode_h

namespace blink {

// This mode corrensponds to inputmode
// https://html.spec.whatwg.org/#attr-fe-inputmode
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.web
// GENERATED_JAVA_PREFIX_TO_STRIP: WebTextInputMode
enum WebTextInputMode {
  kWebTextInputModeDefault,
  kWebTextInputModeNone,
  kWebTextInputModeText,
  kWebTextInputModeTel,
  kWebTextInputModeUrl,
  kWebTextInputModeEmail,
  kWebTextInputModeNumeric,
  kWebTextInputModeDecimal,
  kWebTextInputModeSearch,
  kWebTextInputModeMax = kWebTextInputModeSearch,
};

}  // namespace blink

#endif  // WebTextInputMode_h
