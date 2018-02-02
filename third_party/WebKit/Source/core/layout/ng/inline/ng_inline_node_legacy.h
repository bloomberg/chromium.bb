// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineNodeLegacy_h
#define NGInlineNodeLegacy_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/line/LineInfo.h"
#include "core/layout/line/RootInlineBox.h"

namespace blink {

class NGInlineNode;

// Helper class actiong as a bridge between LayoutNG inline nodes and legacy
// layout. Populates a LayoutBlockFlow with LayoutNG inline layout results.
class CORE_EXPORT NGInlineNodeLegacy {
 public:
  NGInlineNodeLegacy(const NGInlineNode& node) : inline_node_(node) {}

  // Copy fragment data of all lines to LayoutBlockFlow.
  void CopyFragmentDataToLayoutBox(const NGConstraintSpace&,
                                   const NGLayoutResult&);

 private:
  const NGInlineNode& inline_node_;
};

}  // namespace blink

#endif  // NGInlineNodeLegacy_h
