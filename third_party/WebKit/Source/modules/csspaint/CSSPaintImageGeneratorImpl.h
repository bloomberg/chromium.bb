// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintImageGeneratorImpl_h
#define CSSPaintImageGeneratorImpl_h

#include "core/css/CSSPaintImageGenerator.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class CSSPaintDefinition;
class CSSSyntaxDescriptor;
class Document;
class Image;

class CSSPaintImageGeneratorImpl final : public CSSPaintImageGenerator {
 public:
  static CSSPaintImageGenerator* Create(const String& name,
                                        const Document&,
                                        Observer*);
  ~CSSPaintImageGeneratorImpl() override;

  PassRefPtr<Image> Paint(const ImageResourceObserver&,
                          const IntSize&,
                          const CSSStyleValueVector*) final;
  const Vector<CSSPropertyID>& NativeInvalidationProperties() const final;
  const Vector<AtomicString>& CustomInvalidationProperties() const final;
  bool HasAlpha() const final;
  const Vector<CSSSyntaxDescriptor>& InputArgumentTypes() const final;
  bool IsImageGeneratorReady() const final;

  // Should be called from the PaintWorkletGlobalScope when a javascript class
  // is registered with the same name.
  void SetDefinition(CSSPaintDefinition*);

  DECLARE_VIRTUAL_TRACE();

 private:
  CSSPaintImageGeneratorImpl(Observer*);
  CSSPaintImageGeneratorImpl(CSSPaintDefinition*);

  Member<CSSPaintDefinition> definition_;
  Member<Observer> observer_;
};

}  // namespace blink

#endif  // CSSPaintImageGeneratorImpl_h
