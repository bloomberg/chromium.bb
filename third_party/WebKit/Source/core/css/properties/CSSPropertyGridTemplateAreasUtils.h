// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyGridTemplateAreasUtils_h
#define CSSPropertyGridTemplateAreasUtils_h

#include "core/style/GridArea.h"
#include "platform/wtf/Allocator.h"

namespace WTF {
class String;
}

namespace blink {

class CSSPropertyGridTemplateAreasUtils {
  STATIC_ONLY(CSSPropertyGridTemplateAreasUtils);

 public:
  static bool ParseGridTemplateAreasRow(const WTF::String& grid_row_names,
                                        NamedGridAreaMap&,
                                        const size_t row_count,
                                        size_t& column_count);
};

}  // namespace blink

#endif  // CSSPropertyGridTemplateAreasUtils_h
