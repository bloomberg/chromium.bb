// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontTestHelpers_h
#define FontTestHelpers_h

#include "platform/fonts/FontDescription.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Font;

namespace testing {

// Reads a font from a specified path, for use in unit tests only.
Font CreateTestFont(const AtomicString& family_name,
                    const String& font_path,
                    float size,
                    const FontDescription::VariantLigatures* = nullptr);

}  // namespace testing
}  // namespace blink

#endif  // FontTestHelpers_h
