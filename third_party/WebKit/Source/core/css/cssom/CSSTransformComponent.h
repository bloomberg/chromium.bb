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

// CSSTransformComponent is the base class used for the representations of
// the individual CSS transforms. They are combined in a CSSTransformValue
// before they can be used as a value for properties like "transform".
// See CSSTransformComponent.idl for more information about this class.
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
  };

  virtual ~CSSTransformComponent() {}

  // Blink-internal ways of creating CSSTransformComponents.
  static CSSTransformComponent* FromCSSValue(const CSSValue&);

  // Getters and setters for attributes defined in the IDL.
  bool is2D() const { return is2D_; }
  virtual void setIs2D(bool is2D) { is2D_ = is2D; }
  virtual String toString() const {
    const CSSValue* result = ToCSSValue();
    // TODO(meade): Remove this once all the number and length types are
    // rewritten.
    return result ? result->CssText() : "";
  }

  // Internal methods.
  virtual TransformComponentType GetType() const = 0;
  virtual CSSFunctionValue* ToCSSValue() const = 0;
  virtual DOMMatrix* AsMatrix() const = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  CSSTransformComponent(bool is2D) : is2D_(is2D) {}

 private:
  bool is2D_;
};

}  // namespace blink

#endif
