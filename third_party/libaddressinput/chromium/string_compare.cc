// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpp/src/util/string_compare.h"

#include "base/i18n/string_compare.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "cpp/include/libaddressinput/util/scoped_ptr.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace i18n {
namespace addressinput {

bool LooseStringCompare(const std::string& a, const std::string& b) {
  UErrorCode error_code = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(error_code));
  // Differences in diacriticals and case are ignored.
  collator->setStrength(icu::Collator::PRIMARY);
  DCHECK(U_SUCCESS(error_code));
  UCollationResult result = base::i18n::CompareString16WithCollator(
      collator.get(),
      base::UTF8ToUTF16(a),
      base::UTF8ToUTF16(b));
  DCHECK(U_SUCCESS(error_code));
  return result == UCOL_EQUAL;
}

}  // namespace addressinput
}  // namespace i18n
