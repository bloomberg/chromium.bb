// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGTableCell_h
#define LayoutNGTableCell_h

#include "core/CoreExport.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/ng/layout_ng_mixin.h"

namespace blink {

class CORE_EXPORT LayoutNGTableCell final
    : public LayoutNGMixin<LayoutTableCell> {
 public:
  explicit LayoutNGTableCell(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override { return "LayoutNGTableCell"; }
};

}  // namespace blink

#endif  // LayoutNGTableCell_h
