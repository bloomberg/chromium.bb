// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveStyleSheets_h
#define ActiveStyleSheets_h

#include "core/CoreExport.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class CSSStyleSheet;
class RuleSet;

using ActiveStyleSheet = std::pair<Member<CSSStyleSheet>, Member<RuleSet>>;
using ActiveStyleSheetVector = HeapVector<ActiveStyleSheet>;

enum ActiveSheetsChange {
  kNoActiveSheetsChanged,  // Nothing changed.
  kActiveSheetsChanged,    // Sheets were added and/or inserted.
  kActiveSheetsAppended    // Only additions, and all appended.
};

CORE_EXPORT ActiveSheetsChange
CompareActiveStyleSheets(const ActiveStyleSheetVector& old_style_sheets,
                         const ActiveStyleSheetVector& new_style_sheets,
                         HeapHashSet<Member<RuleSet>>& changed_rule_sets);

bool ClearMediaQueryDependentRuleSets(
    const ActiveStyleSheetVector& active_style_sheets);

}  // namespace blink

#endif  // ActiveStyleSheets_h
