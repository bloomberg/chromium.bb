/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/LocalCaretRect.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/InlineBoxPosition.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/RootInlineBox.h"

namespace blink {

namespace {

LocalCaretRect ComputeLocalCaretRect(const LayoutObject* layout_object,
                                     const InlineBoxPosition box_position) {
  return LocalCaretRect(
      layout_object, layout_object->LocalCaretRect(box_position.inline_box,
                                                   box_position.offset_in_box));
}

template <typename Strategy>
LocalCaretRect LocalCaretRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  LayoutObject* const layout_object = node->GetLayoutObject();
  if (!layout_object)
    return LocalCaretRect();

  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position);

  if (adjusted.IsNotNull()) {
    // TODO(xiaochengh): Plug in NG implementation here.

    // TODO(editing-dev): This DCHECK is for ensuring the correctness of
    // breaking |ComputeInlineBoxPosition| into |ComputeInlineAdjustedPosition|
    // and |ComputeInlineBoxPositionForInlineAdjustedPosition|. If there is any
    // DCHECK hit, we should pass primary direction to the latter function.
    // TODO(crbug.com/793098): Fix it so that we don't need to bother about
    // primary direction.
    DCHECK_EQ(PrimaryDirectionOf(*position.AnchorNode()),
              PrimaryDirectionOf(*adjusted.AnchorNode()));
    const InlineBoxPosition& box_position =
        ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);

    if (box_position.inline_box) {
      return ComputeLocalCaretRect(
          LineLayoutAPIShim::LayoutObjectFrom(
              box_position.inline_box->GetLineLayoutItem()),
          box_position);
    }
  }

  // DeleteSelectionCommandTest.deleteListFromTable goes here.
  return LocalCaretRect(
      layout_object,
      layout_object->LocalCaretRect(
          nullptr, position.GetPosition().ComputeEditingOffset()));
}

// This function was added because the caret rect that is calculated by
// using the line top value instead of the selection top.
template <typename Strategy>
LocalCaretRect LocalSelectionRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  if (!node->GetLayoutObject())
    return LocalCaretRect();

  const PositionWithAffinityTemplate<Strategy>& adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return LocalCaretRect();

  // TODO(xiaochengh): Plug in NG implementation here.

  // TODO(editing-dev): This DCHECK is for ensuring the correctness of
  // breaking |ComputeInlineBoxPosition| into |ComputeInlineAdjustedPosition|
  // and |ComputeInlineBoxPositionForInlineAdjustedPosition|. If there is any
  // DCHECK hit, we should pass primary direction to the latter function.
  // TODO(crbug.com/793098): Fix it so that we don't need to bother about
  // primary direction.
  DCHECK_EQ(PrimaryDirectionOf(*position.AnchorNode()),
            PrimaryDirectionOf(*adjusted.AnchorNode()));
  const InlineBoxPosition& box_position =
      ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted);

  if (!box_position.inline_box)
    return LocalCaretRect();

  LayoutObject* const layout_object = LineLayoutAPIShim::LayoutObjectFrom(
      box_position.inline_box->GetLineLayoutItem());

  const LayoutRect& rect = layout_object->LocalCaretRect(
      box_position.inline_box, box_position.offset_in_box);

  if (rect.IsEmpty())
    return LocalCaretRect();

  const InlineBox* const box = box_position.inline_box;
  if (layout_object->Style()->IsHorizontalWritingMode()) {
    return LocalCaretRect(
        layout_object,
        LayoutRect(LayoutPoint(rect.X(), box->Root().SelectionTop()),
                   LayoutSize(rect.Width(), box->Root().SelectionHeight())));
  }

  return LocalCaretRect(
      layout_object,
      LayoutRect(LayoutPoint(box->Root().SelectionTop(), rect.Y()),
                 LayoutSize(box->Root().SelectionHeight(), rect.Height())));
}

}  // namespace

LocalCaretRect LocalCaretRectOfPosition(const PositionWithAffinity& position) {
  return LocalCaretRectOfPositionTemplate<EditingStrategy>(position);
}

LocalCaretRect LocalCaretRectOfPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return LocalCaretRectOfPositionTemplate<EditingInFlatTreeStrategy>(position);
}

LocalCaretRect LocalSelectionRectOfPosition(
    const PositionWithAffinity& position) {
  return LocalSelectionRectOfPositionTemplate<EditingStrategy>(position);
}

}  // namespace blink
