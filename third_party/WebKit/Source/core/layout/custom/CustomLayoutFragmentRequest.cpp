// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/CustomLayoutFragmentRequest.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/custom/CustomLayoutChild.h"
#include "core/layout/custom/CustomLayoutConstraintsOptions.h"
#include "core/layout/custom/CustomLayoutFragment.h"

namespace blink {

CustomLayoutFragmentRequest::CustomLayoutFragmentRequest(
    CustomLayoutChild* child,
    const CustomLayoutConstraintsOptions& options)
    : child_(child), options_(options) {}

CustomLayoutFragment* CustomLayoutFragmentRequest::PerformLayout() {
  // Abort if the child we are trying to perform layout upon doesn't exist.
  if (!IsValid())
    return nullptr;

  LayoutBox* box = child_->GetLayoutBox();
  const ComputedStyle& style = box->StyleRef();

  // TODO(ikilpatrick): At the moment we just pretend that we are being sized
  // off something which is 0x0. Additional fields inside the constraints
  // object will allow the developer to override this.
  box->SetOverrideContainingBlockContentLogicalWidth(LayoutUnit());
  box->SetOverrideContainingBlockContentLogicalHeight(LayoutUnit());

  DCHECK(box->Parent());
  DCHECK(box->Parent()->IsLayoutCustom());
  DCHECK(box->Parent() == box->ContainingBlock());

  LayoutObject* parent = box->Parent();

  bool is_parallel_writing_mode = IsParallelWritingMode(
      parent->StyleRef().GetWritingMode(), style.GetWritingMode());

  if (options_.hasFixedInlineSize()) {
    if (is_parallel_writing_mode) {
      box->SetOverrideLogicalContentWidth(
          (LayoutUnit(options_.fixedInlineSize()) -
           box->BorderAndPaddingLogicalWidth())
              .ClampNegativeToZero());
    } else {
      box->SetOverrideLogicalContentHeight(
          (LayoutUnit(options_.fixedInlineSize()) -
           box->BorderAndPaddingLogicalHeight())
              .ClampNegativeToZero());
    }
  }

  if (options_.hasFixedBlockSize()) {
    if (is_parallel_writing_mode) {
      box->SetOverrideLogicalContentHeight(
          (LayoutUnit(options_.fixedBlockSize()) -
           box->BorderAndPaddingLogicalHeight())
              .ClampNegativeToZero());
    } else {
      box->SetOverrideLogicalContentWidth(
          (LayoutUnit(options_.fixedBlockSize()) -
           box->BorderAndPaddingLogicalWidth())
              .ClampNegativeToZero());
    }
  }

  box->ForceLayout();

  box->ClearContainingBlockOverrideSize();
  box->ClearOverrideSize();

  LayoutUnit fragment_inline_size =
      is_parallel_writing_mode ? box->LogicalWidth() : box->LogicalHeight();
  LayoutUnit fragment_block_size =
      is_parallel_writing_mode ? box->LogicalHeight() : box->LogicalWidth();

  return new CustomLayoutFragment(this, fragment_inline_size,
                                  fragment_block_size);
}

LayoutBox* CustomLayoutFragmentRequest::GetLayoutBox() const {
  return child_->GetLayoutBox();
}

bool CustomLayoutFragmentRequest::IsValid() const {
  return child_->IsValid();
}

void CustomLayoutFragmentRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(child_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
