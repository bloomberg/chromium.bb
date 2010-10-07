// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/i18n/time_formatting.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

using base::Time;

namespace printing {

void PrintedDocument::Immutable::SetDocumentDate() {
  Time now = Time::Now();
  date_ = WideToUTF16Hack(base::TimeFormatShortDateNumeric(now));
  time_ = WideToUTF16Hack(base::TimeFormatTimeOfDay(now));
}

}  // namespace printing
