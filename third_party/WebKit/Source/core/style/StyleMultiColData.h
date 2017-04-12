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

#ifndef StyleMultiColData_h
#define StyleMultiColData_h

#include "core/CoreExport.h"
#include "core/style/BorderValue.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

// CSS3 Multi Column Layout

class CORE_EXPORT StyleMultiColData : public RefCounted<StyleMultiColData> {
 public:
  static PassRefPtr<StyleMultiColData> Create() {
    return AdoptRef(new StyleMultiColData);
  }
  PassRefPtr<StyleMultiColData> Copy() const {
    return AdoptRef(new StyleMultiColData(*this));
  }

  bool operator==(const StyleMultiColData&) const;
  bool operator!=(const StyleMultiColData& o) const { return !(*this == o); }

  unsigned short RuleWidth() const {
    if (rule_.Style() == kBorderStyleNone ||
        rule_.Style() == kBorderStyleHidden)
      return 0;
    return rule_.Width();
  }

  float width_;
  unsigned short count_;
  float gap_;
  BorderValue rule_;
  StyleColor visited_link_column_rule_color_;

  unsigned auto_width_ : 1;
  unsigned auto_count_ : 1;
  unsigned normal_gap_ : 1;
  unsigned fill_ : 1;  // ColumnFill
  unsigned column_span_ : 1;

 private:
  StyleMultiColData();
  StyleMultiColData(const StyleMultiColData&);
};

}  // namespace blink

#endif  // StyleMultiColData_h
