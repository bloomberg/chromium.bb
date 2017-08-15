// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutSliderItem_h
#define LayoutSliderItem_h

#include "core/CoreExport.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutSlider;

class CORE_EXPORT LayoutSliderItem : public LayoutBlockItem {
 public:
  explicit LayoutSliderItem(LayoutSlider*);

  explicit LayoutSliderItem(const LayoutBlockItem&);

  explicit LayoutSliderItem(std::nullptr_t);

  LayoutSliderItem();

  bool InDragMode() const;

 private:
  LayoutSlider* ToSlider();
  const LayoutSlider* ToSlider() const;
};

}  // namespace blink

#endif  // LayoutSliderItem_h
