// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/font_fallback.h"

#include <set>
#include <string>

#include "base/i18n/char_iterator.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace vr {

std::set<UChar32> CollectDifferentChars(base::string16 text) {
  std::set<UChar32> characters;
  for (base::i18n::UTF16CharIterator it(&text); !it.end(); it.Advance()) {
    characters.insert(it.get());
  }
  return characters;
}

}  // namespace vr
