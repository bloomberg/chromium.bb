// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"

namespace blink {

NGFragmentBuilder::NGFragmentBuilder(NGFragmentBase::NGFragmentType type)
    : type_(type),
      writing_mode_(HorizontalTopBottom),
      direction_(LeftToRight) {}

NGFragmentBuilder& NGFragmentBuilder::SetWritingMode(
    NGWritingMode writing_mode) {
  writing_mode_ = writing_mode;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetDirection(NGDirection direction) {
  direction_ = direction;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetInlineSize(LayoutUnit size) {
  size_.inlineSize = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockSize(LayoutUnit size) {
  size_.blockSize = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetInlineOverflow(LayoutUnit size) {
  overflow_.inlineSize = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockOverflow(LayoutUnit size) {
  overflow_.blockSize = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddChild(const NGFragment* child) {
  DCHECK_EQ(type_, NGFragmentBase::FragmentBox)
      << "Only box fragments can have children";
  children_.append(child);
  return *this;
}

NGFragment* NGFragmentBuilder::ToFragment() {
  // TODO(layout-ng): Support text fragments
  DCHECK_EQ(type_, NGFragmentBase::FragmentBox);
  NGFragment* fragment =
      new NGFragment(size_, overflow_, writing_mode_, direction_, children_);
  return fragment;
}

}  // namespace blink
