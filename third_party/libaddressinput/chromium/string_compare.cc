// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/util/string_compare.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace i18n {
namespace addressinput {

namespace {

class IcuStringComparer {
 public:
  IcuStringComparer() {
    UErrorCode error_code = U_ZERO_ERROR;
    collator_.reset(
        icu::Collator::createInstance(icu::Locale::getRoot(), error_code));
    DCHECK(U_SUCCESS(error_code));
    collator_->setStrength(icu::Collator::PRIMARY);
  }

  ~IcuStringComparer() {}

  int Compare(const std::string& a, const std::string& b) const {
    UErrorCode error_code = U_ZERO_ERROR;
    int result = collator_->compareUTF8(a, b, error_code);
    DCHECK(U_SUCCESS(error_code));
    return result;
  }

 private:
  // ::scoped_ptr is from "base/memory/scoped_ptr.h", which does not interfere
  // with ::i18n::addressinput::scoped_ptr from
  // <libaddressinput/util/scoped_ptr.h>.
  ::scoped_ptr<icu::Collator> collator_;

  DISALLOW_COPY_AND_ASSIGN(IcuStringComparer);
};

static base::LazyInstance<IcuStringComparer> g_comparer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Dummy required for scoped_ptr<Impl>.
class StringCompare::Impl {};

StringCompare::StringCompare() {}

StringCompare::~StringCompare() {}

bool StringCompare::NaturalEquals(const std::string& a,
                                  const std::string& b) const {
  return g_comparer.Get().Compare(a, b) == 0;
}

bool StringCompare::NaturalLess(const std::string& a,
                                const std::string& b) const {
  return g_comparer.Get().Compare(a, b) < 0;
}

}  // namespace addressinput
}  // namespace i18n
