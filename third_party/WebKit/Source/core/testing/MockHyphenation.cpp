// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/MockHyphenation.h"

namespace blink {

size_t MockHyphenation::lastHyphenLocation(const StringView& text,
    size_t beforeIndex) const
{
    RefPtr<StringImpl> str = text.toString();
    if (str->endsWithIgnoringASCIICase("phenation", 9)) {
        if (beforeIndex - (str->length() - 9) > 4)
            return 4 + (str->length() - 9);
        if (str->endsWithIgnoringASCIICase("hyphenation", 11)
            && beforeIndex - (str->length() - 11) > 2) {
            return 2 + (str->length() - 11);
        }
    }
    return 0;
}

} // namespace blink
