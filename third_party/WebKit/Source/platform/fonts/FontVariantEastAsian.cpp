// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontVariantEastAsian.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

static const char* kUnknownString = "Unknown";

String FontVariantEastAsian::ToString(EastAsianForm form) {
  switch (form) {
    case EastAsianForm::kNormalForm:
      return "Normal";
    case EastAsianForm::kJis78:
      return "Jis78";
    case EastAsianForm::kJis83:
      return "Jis83";
    case EastAsianForm::kJis90:
      return "Jis90";
    case EastAsianForm::kJis04:
      return "Jis04";
    case EastAsianForm::kSimplified:
      return "Simplified";
    case EastAsianForm::kTraditional:
      return "Traditional";
  }
  return kUnknownString;
}

String FontVariantEastAsian::ToString(EastAsianWidth width) {
  switch (width) {
    case FontVariantEastAsian::kNormalWidth:
      return "Normal";
    case FontVariantEastAsian::kFullWidth:
      return "Full";
    case FontVariantEastAsian::kProportionalWidth:
      return "Proportional";
  }
  return kUnknownString;
}

String FontVariantEastAsian::ToString() const {
  return String::Format(
      "form=%s, width=%s, ruby=%s", ToString(Form()).Ascii().data(),
      ToString(Width()).Ascii().data(), Ruby() ? "true" : "false");
}

}  // namespace blink
