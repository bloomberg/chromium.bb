// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewFragmentationContext_h
#define ViewFragmentationContext_h

#include "core/layout/FragmentationContext.h"

namespace blink {

class LayoutView;

class ViewFragmentationContext final : public FragmentationContext {
 public:
  ViewFragmentationContext(LayoutView& view) : view_(view) {}
  bool IsFragmentainerLogicalHeightKnown() final;
  LayoutUnit FragmentainerLogicalHeightAt(LayoutUnit block_offset) final;
  LayoutUnit RemainingLogicalHeightAt(LayoutUnit block_offset) final;

 private:
  LayoutView& view_;
};

}  // namespace blink

#endif  // ViewFragmentationContext_h
