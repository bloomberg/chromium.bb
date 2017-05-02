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

#ifndef StyleBackgroundData_h
#define StyleBackgroundData_h

#include "core/css/StyleColor.h"
#include "core/style/FillLayer.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class CORE_EXPORT StyleBackgroundData : public RefCounted<StyleBackgroundData> {
 public:
  static PassRefPtr<StyleBackgroundData> Create() {
    return AdoptRef(new StyleBackgroundData);
  }
  PassRefPtr<StyleBackgroundData> Copy() const {
    return AdoptRef(new StyleBackgroundData(*this));
  }
  ~StyleBackgroundData() {}

  bool operator==(const StyleBackgroundData&) const;
  bool operator!=(const StyleBackgroundData& o) const { return !(*this == o); }

  const FillLayer& Background() const { return background_; }
  const StyleColor& GetColor() const { return color_; }

 private:
  friend class ComputedStyle;

  StyleBackgroundData();
  StyleBackgroundData(const StyleBackgroundData&);

  FillLayer background_;
  StyleColor color_;
};

}  // namespace blink

#endif  // StyleBackgroundData_h
