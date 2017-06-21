/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef CollapsedBorderValue_h
#define CollapsedBorderValue_h

#include "core/style/ComputedStyle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

enum EBorderPrecedence {
  kBorderPrecedenceOff,
  kBorderPrecedenceTable,
  kBorderPrecedenceColumnGroup,
  kBorderPrecedenceColumn,
  kBorderPrecedenceRowGroup,
  kBorderPrecedenceRow,
  kBorderPrecedenceCell
};

class CollapsedBorderValue {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  // Constructs a CollapsedBorderValue for non-existence border.
  CollapsedBorderValue()
      : color_(0),
        width_(0),
        style_(static_cast<unsigned>(EBorderStyle::kNone)),
        precedence_(kBorderPrecedenceOff) {}

  CollapsedBorderValue(const BorderValue& border,
                       const Color& color,
                       EBorderPrecedence precedence)
      : color_(color),
        width_(ComputedStyle::BorderStyleIsVisible(border.Style())
                   ? border.Width()
                   : 0),
        style_(static_cast<unsigned>(border.Style())),
        precedence_(precedence) {
    DCHECK(precedence != kBorderPrecedenceOff);
  }

  CollapsedBorderValue(EBorderStyle style,
                       const float width,
                       const Color& color,
                       EBorderPrecedence precedence)
      : color_(color),
        width_(ComputedStyle::BorderStyleIsVisible(style) ? width : 0),
        style_(static_cast<unsigned>(style)),
        precedence_(precedence) {
    DCHECK(precedence != kBorderPrecedenceOff);
  }

  unsigned Width() const { return width_; }
  EBorderStyle Style() const { return static_cast<EBorderStyle>(style_); }
  bool Exists() const { return precedence_ != kBorderPrecedenceOff; }
  Color GetColor() const { return color_; }
  bool IsTransparent() const { return !color_.Alpha(); }
  EBorderPrecedence Precedence() const {
    return static_cast<EBorderPrecedence>(precedence_);
  }

  bool IsSameIgnoringColor(const CollapsedBorderValue& o) const {
    return Width() == o.Width() && Style() == o.Style() &&
           Precedence() == o.Precedence();
  }

  bool VisuallyEquals(const CollapsedBorderValue& o) const {
    if (!IsVisible() && !o.IsVisible())
      return true;
    return GetColor() == o.GetColor() && Width() == o.Width() &&
           Style() == o.Style();
  }

  bool IsVisible() const { return Width() && !IsTransparent(); }

  // Compares the precedence of two borders for conflict resolution. Returns
  // true if |this| has lower precedence than |other|.
  //
  // The following rules apply for resolving conflicts and figuring out which
  // border to use:
  // (See https://www.w3.org/TR/CSS2/tables.html#border-conflict-resolution)
  // (1) Borders with the 'border-style' of 'hidden' take precedence over all
  //     other conflicting borders. Any border with this value suppresses all
  //     borders at this location.
  // (2) Borders with a style of 'none' have the lowest priority. Only if the
  //     border properties of all the elements meeting at this edge are 'none'
  //     will the border be omitted (but note that 'none' is the default value
  //     for the border style.)
  // (3) If none of the styles are 'hidden' and at least one of them is not
  //     'none', then narrow borders are discarded in favor of wider ones. If
  //      several have the same 'border-width' then styles are preferred in this
  //      order: 'double', 'solid', 'dashed', 'dotted', 'ridge', 'outset',
  //     'groove', and the lowest: 'inset'.
  // (4) If border styles differ only in color, then a style set on a cell wins
  //     over one on a row, which wins over a row group, column, column group
  //     and, lastly, table. It is undefined which color is used when two
  //     elements of the same type disagree.
  bool LessThan(const CollapsedBorderValue& other) const {
    // Sanity check the values passed in. The null border have lowest priority.
    if (!other.Exists())
      return false;
    if (!Exists())
      return true;

    // Rule #1 above.
    if (Style() == EBorderStyle::kHidden)
      return false;
    if (other.Style() == EBorderStyle::kHidden)
      return true;

    // Rule #2 above.  A style of 'none' has lowest priority and always loses to
    // any other border.
    if (other.Style() == EBorderStyle::kNone)
      return false;
    if (Style() == EBorderStyle::kNone)
      return true;

    // The first part of rule #3 above. Wider borders win.
    if (Width() != other.Width())
      return Width() < other.Width();

    // The borders have equal width.  Sort by border style.
    if (Style() != other.Style())
      return Style() < other.Style();

    // The border have the same width and style.  Rely on precedence (cell over
    // row over row group, etc.)
    return Precedence() < other.Precedence();
  }

  // Suppose |this| and |other| are adjoining borders forming a joint, this
  // method returns true if |this| should cover the joint. The rule similar to
  // LessThan() except that invisible borders always lose.
  bool CoversJoint(const CollapsedBorderValue& other) const {
    return IsVisible() && (!other.IsVisible() || !LessThan(other));
  }

  // A border may form a joint with other 3 borders. Returns true if this border
  // wins to cover the joint.
  bool CoversJoint(const CollapsedBorderValue* other1,
                   const CollapsedBorderValue* other2,
                   const CollapsedBorderValue* other3) const {
    return (!other1 || CoversJoint(*other1)) &&
           (!other2 || CoversJoint(*other2)) &&
           (!other3 || CoversJoint(*other3));
  }

 private:
  Color color_;
  unsigned width_ : 25;
  unsigned style_ : 4;       // EBorderStyle
  unsigned precedence_ : 3;  // EBorderPrecedence
};

}  // namespace blink

#endif  // CollapsedBorderValue_h
