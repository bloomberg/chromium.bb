/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutSelection_h
#define LayoutSelection_h

#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "platform/heap/Handle.h"

namespace blink {

class FrameSelection;

class LayoutSelection final : public GarbageCollected<LayoutSelection> {
 public:
  static LayoutSelection* Create(FrameSelection& frame_selection) {
    return new LayoutSelection(frame_selection);
  }

  bool HasPendingSelection() const { return has_pending_selection_; }
  void SetHasPendingSelection() { has_pending_selection_ = true; }
  void Commit();

  IntRect SelectionBounds();
  void InvalidatePaintForSelection();
  enum SelectionPaintInvalidationMode {
    kPaintInvalidationNewXOROld,
    kPaintInvalidationNewMinusOld
  };
  void SetSelection(
      LayoutObject* start,
      int start_pos,
      LayoutObject*,
      int end_pos,
      SelectionPaintInvalidationMode = kPaintInvalidationNewXOROld);
  void ClearSelection();
  std::pair<int, int> SelectionStartEnd();
  void OnDocumentShutdown();

  DECLARE_TRACE();

 private:
  LayoutSelection(FrameSelection&);

  SelectionInFlatTree CalcVisibleSelection(
      const VisibleSelectionInFlatTree&) const;

  Member<FrameSelection> frame_selection_;
  bool has_pending_selection_ : 1;

  // The current selection represented as 2 boundaries.
  // Selection boundaries are represented in LayoutView by a tuple
  // (LayoutObject, DOM node offset).
  // See http://www.w3.org/TR/dom/#range for more information.
  //
  // |m_selectionStartPos| and |m_selectionEndPos| are only valid for
  // |Text| node without 'transform' or 'first-letter'.
  //
  // Those are used for selection painting and paint invalidation upon
  // selection change.
  LayoutObject* selection_start_;
  LayoutObject* selection_end_;

  // TODO(yosin): Clarify the meaning of these variables. editing/ passes
  // them as offsets in the DOM tree  but layout uses them as offset in the
  // layout tree.
  int selection_start_pos_;
  int selection_end_pos_;
};

}  // namespace blink

#endif
