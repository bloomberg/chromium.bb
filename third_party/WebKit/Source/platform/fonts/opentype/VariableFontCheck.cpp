// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/opentype/VariableFontCheck.h"

#include "SkTypeface.h"
#include "platform/wtf/Vector.h"

namespace blink {

bool VariableFontCheck::IsVariableFont(SkTypeface* typeface) {
  const size_t table_count = typeface->countTables();
  Vector<SkFontTableTag> table_tags;
  table_tags.resize(table_count);
  int table_tags_result = typeface->getTableTags(table_tags.data());
  return table_tags_result && table_tags.Contains(SkFontTableTag(
                                  SkSetFourByteTag('f', 'v', 'a', 'r')));
}

}  // namespace blink
