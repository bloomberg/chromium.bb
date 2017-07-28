// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BasicShapePropertyFunctions_h
#define BasicShapePropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/BasicShapes.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;

class BasicShapePropertyFunctions {
  STATIC_ONLY(BasicShapePropertyFunctions);

 public:
  static const BasicShape* GetInitialBasicShape(CSSPropertyID property) {
    return GetBasicShape(property, ComputedStyle::InitialStyle());
  }

  static const BasicShape* GetBasicShape(CSSPropertyID property,
                                         const ComputedStyle& style) {
    switch (property) {
      case CSSPropertyShapeOutside:
        if (!style.ShapeOutside())
          return nullptr;
        if (style.ShapeOutside()->GetType() != ShapeValue::kShape)
          return nullptr;
        if (style.ShapeOutside()->CssBox() != kBoxMissing)
          return nullptr;
        return style.ShapeOutside()->Shape();
      case CSSPropertyClipPath:
        if (!style.ClipPath())
          return nullptr;
        if (style.ClipPath()->GetType() != ClipPathOperation::SHAPE)
          return nullptr;
        return ToShapeClipPathOperation(style.ClipPath())->GetBasicShape();
      default:
        NOTREACHED();
        return nullptr;
    }
  }

  static void SetBasicShape(CSSPropertyID property,
                            ComputedStyle& style,
                            PassRefPtr<BasicShape> shape) {
    switch (property) {
      case CSSPropertyShapeOutside:
        style.SetShapeOutside(
            ShapeValue::CreateShapeValue(std::move(shape), kBoxMissing));
        break;
      case CSSPropertyClipPath:
        style.SetClipPath(ShapeClipPathOperation::Create(std::move(shape)));
        break;
      default:
        NOTREACHED();
    }
  }
};

}  // namespace blink

#endif  // BasicShapePropertyFunctions_h
