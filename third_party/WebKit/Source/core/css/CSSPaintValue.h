// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintValue_h
#define CSSPaintValue_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "core/css/CSSVariableData.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CSSPaintValue : public CSSImageGeneratorValue {
 public:
  static CSSPaintValue* Create(CSSCustomIdentValue* name) {
    return new CSSPaintValue(name);
  }

  static CSSPaintValue* Create(CSSCustomIdentValue* name,
                               Vector<RefPtr<CSSVariableData>>& variable_data) {
    return new CSSPaintValue(name, variable_data);
  }

  ~CSSPaintValue();

  String CustomCSSText() const;

  String GetName() const;

  PassRefPtr<Image> GetImage(const ImageResourceObserver&,
                             const Document&,
                             const ComputedStyle&,
                             const IntSize&);
  bool IsFixedSize() const { return false; }
  IntSize FixedSize(const Document&) { return IntSize(); }

  bool IsPending() const { return true; }
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const;

  void LoadSubimages(const Document&) {}

  bool Equals(const CSSPaintValue&) const;

  const Vector<CSSPropertyID>* NativeInvalidationProperties() const {
    return generator_ ? &generator_->NativeInvalidationProperties() : nullptr;
  }
  const Vector<AtomicString>* CustomInvalidationProperties() const {
    return generator_ ? &generator_->CustomInvalidationProperties() : nullptr;
  }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  explicit CSSPaintValue(CSSCustomIdentValue* name);

  CSSPaintValue(CSSCustomIdentValue* name, Vector<RefPtr<CSSVariableData>>&);

  class Observer final : public CSSPaintImageGenerator::Observer {
    WTF_MAKE_NONCOPYABLE(Observer);

   public:
    explicit Observer(CSSPaintValue* owner_value) : owner_value_(owner_value) {}

    ~Observer() override {}
    DEFINE_INLINE_VIRTUAL_TRACE() {
      visitor->Trace(owner_value_);
      CSSPaintImageGenerator::Observer::Trace(visitor);
    }

    void PaintImageGeneratorReady() final;

   private:
    Member<CSSPaintValue> owner_value_;
  };

  void PaintImageGeneratorReady();

  bool ParseInputArguments();

  bool input_arguments_invalid_ = false;

  Member<CSSCustomIdentValue> name_;
  Member<CSSPaintImageGenerator> generator_;
  Member<Observer> paint_image_generator_observer_;
  Member<CSSStyleValueVector> parsed_input_arguments_;
  Vector<RefPtr<CSSVariableData>> argument_variable_data_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPaintValue, IsPaintValue());

}  // namespace blink

#endif  // CSSPaintValue_h
