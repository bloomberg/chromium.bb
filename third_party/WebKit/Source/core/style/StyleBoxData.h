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

#ifndef StyleBoxData_h
#define StyleBoxData_h

#include "core/CoreExport.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Length.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class CORE_EXPORT StyleBoxData : public RefCountedCopyable<StyleBoxData> {
 public:
  static PassRefPtr<StyleBoxData> Create() {
    return AdoptRef(new StyleBoxData);
  }
  PassRefPtr<StyleBoxData> Copy() const {
    return AdoptRef(new StyleBoxData(*this));
  }

  bool operator==(const StyleBoxData&) const;
  bool operator!=(const StyleBoxData& o) const { return !(*this == o); }

  const Length& Width() const { return width_; }
  const Length& Height() const { return height_; }

  const Length& MinWidth() const { return min_width_; }
  const Length& MinHeight() const { return min_height_; }

  const Length& MaxWidth() const { return max_width_; }
  const Length& MaxHeight() const { return max_height_; }

  const Length& VerticalAlign() const { return vertical_align_; }

  int ZIndex() const { return z_index_; }
  bool HasAutoZIndex() const { return has_auto_z_index_; }

  EBoxSizing BoxSizing() const { return static_cast<EBoxSizing>(box_sizing_); }
  EBoxDecorationBreak BoxDecorationBreak() const {
    return static_cast<EBoxDecorationBreak>(box_decoration_break_);
  }

 private:
  friend class ComputedStyle;

  StyleBoxData();
  StyleBoxData(const StyleBoxData&) = default;

  Length width_;
  Length height_;

  Length min_width_;
  Length max_width_;

  Length min_height_;
  Length max_height_;

  Length vertical_align_;

  int z_index_;
  unsigned has_auto_z_index_ : 1;
  unsigned box_sizing_ : 1;            // EBoxSizing
  unsigned box_decoration_break_ : 1;  // EBoxDecorationBreak
};

}  // namespace blink

#endif  // StyleBoxData_h
