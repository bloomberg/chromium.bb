// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BaselineAlignment_h
#define BaselineAlignment_h

#include "core/layout/LayoutBox.h"

namespace blink {

// These classes are used to implement the Baseline Alignment logica, as
// described in the CSS Box Alignment specification.
// https://drafts.csswg.org/css-align/#baseline-terms

// A baseline-sharing group is composed of boxes that participate in
// baseline alignment together. This is possible only if they:
//
//   * Share an alignment context along an axis perpendicular to their
//   baseline alignment axis.
//   * Have compatible baseline alignment preferences (i.e., the baselines
//   that want to align are on the same side of the alignment context).
class BaselineGroup {
 public:
  void update(const LayoutBox&, LayoutUnit ascent, LayoutUnit descent);
  LayoutUnit maxAscent() const { return m_maxAscent; }
  LayoutUnit maxDescent() const { return m_maxDescent; }
  int size() const { return m_items.size(); }

 private:
  friend class BaselineContext;
  BaselineGroup(WritingMode blockFlow, ItemPosition childPreference);
  bool isCompatible(WritingMode, ItemPosition) const;
  bool isOppositeBlockFlow(WritingMode blockFlow) const;
  bool isOrthogonalBlockFlow(WritingMode blockFlow) const;

  WritingMode m_blockFlow;
  ItemPosition m_preference;
  LayoutUnit m_maxAscent;
  LayoutUnit m_maxDescent;
  HashSet<const LayoutBox*> m_items;
};

// Boxes share an alignment context along a particular axis when they
// are:
//
//  * table cells in the same row, along the table's row (inline) axis
//  * table cells in the same column, along the table's column (block)
//  axis
//  * grid items in the same row, along the grid's row (inline) axis
//  * grid items in the same column, along the grid's colum (block) axis
//  * flex items in the same flex line, along the flex container's main
//  axis
class BaselineContext {
 public:
  BaselineContext(const LayoutBox& child,
                  ItemPosition preference,
                  LayoutUnit ascent,
                  LayoutUnit descent);
  Vector<BaselineGroup>& sharedGroups() { return m_sharedGroups; }
  const BaselineGroup& getSharedGroup(const LayoutBox& child,
                                      ItemPosition preference) const;
  void updateSharedGroup(const LayoutBox& child,
                         ItemPosition preference,
                         LayoutUnit ascent,
                         LayoutUnit descent);

 private:
  // TODO Properly implement baseline-group compatibility
  // See https://github.com/w3c/csswg-drafts/issues/721
  BaselineGroup& findCompatibleSharedGroup(const LayoutBox& child,
                                           ItemPosition preference);

  Vector<BaselineGroup> m_sharedGroups;
};

static inline bool isBaselinePosition(ItemPosition position) {
  return position == ItemPositionBaseline ||
         position == ItemPositionLastBaseline;
}

}  // namespace blink

#endif  // BaselineContext_h
