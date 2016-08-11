// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGBlockFlow_h
#define LayoutNGBlockFlow_h

#include "core/layout/LayoutBlockFlow.h"

namespace blink {

// This overrides the default layout block algorithm to use Layout NG.
class LayoutNGBlockFlow final : public LayoutBlockFlow {
 public:
  explicit LayoutNGBlockFlow(Element*);
  ~LayoutNGBlockFlow() override = default;

  void layoutBlock(bool relayoutChildren) override;

 private:
  bool isOfType(LayoutObjectType) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGBlockFlow, isLayoutNGBlockFlow());

}  // namespace blink

#endif  // LayoutNGBlockFlow_h
