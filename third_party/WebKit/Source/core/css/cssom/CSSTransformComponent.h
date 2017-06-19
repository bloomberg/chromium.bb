// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformComponent_h
#define CSSTransformComponent_h

#include "core/CoreExport.h"
#include "core/css/CSSFunctionValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMMatrix;

class CORE_EXPORT CSSTransformComponent
    : public GarbageCollectedFinalized<CSSTransformComponent>,
      public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CSSTransformComponent);
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum TransformComponentType {
    kMatrixType,
    kPerspectiveType,
    kRotationType,
    kScaleType,
    kSkewType,
    kTranslationType,
    kMatrix3DType,
    kRotation3DType,
    kScale3DType,
    kTranslation3DType
  };

  static CSSTransformComponent* FromCSSValue(const CSSValue&);

  static bool Is2DComponentType(TransformComponentType transform_type) {
    return transform_type != kMatrix3DType &&
           transform_type != kPerspectiveType &&
           transform_type != kRotation3DType &&
           transform_type != kScale3DType &&
           transform_type != kTranslation3DType;
  }

  virtual ~CSSTransformComponent() {}

  virtual TransformComponentType GetType() const = 0;

  bool is2D() const { return Is2DComponentType(GetType()); }

  virtual String toString() const {
    const CSSValue* result = ToCSSValue();
    // TODO(meade): Remove this once all the number and length types are
    // rewritten.
    return result ? result->CssText() : "";
  }

  virtual CSSFunctionValue* ToCSSValue() const = 0;
  virtual DOMMatrix* AsMatrix() const = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  CSSTransformComponent() = default;
};

}  // namespace blink

#endif
