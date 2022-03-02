// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LEGAL_MESSAGE_LINE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LEGAL_MESSAGE_LINE_H_

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {

using Link = LegalMessageLine::Link;

// A legal message line that allows for modifications.
class TestLegalMessageLine : public LegalMessageLine {
 public:
  TestLegalMessageLine() {}

  TestLegalMessageLine(const std::string& ascii_text) { set_text(ascii_text); }

  TestLegalMessageLine(const std::string& ascii_text, const Links& links) {
    set_text(ascii_text);
    set_links(links);
  }

  TestLegalMessageLine(const TestLegalMessageLine&) = delete;
  TestLegalMessageLine& operator=(const TestLegalMessageLine&) = delete;

  ~TestLegalMessageLine() override {}

  void set_text(const std::string& ascii_text) {
    text_ = base::ASCIIToUTF16(ascii_text);
  }

  void set_links(const Links& links) { links_ = links; }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LEGAL_MESSAGE_LINE_H_
