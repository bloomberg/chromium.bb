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

class CSSSyntaxDescriptor;
class Document;
class Image;
class PaintWorklet;

class CSSPaintImageGeneratorImpl final : public CSSPaintImageGenerator {
 public:
  static CSSPaintImageGenerator* Create(const String& name,
                                        const Document&,
                                        Observer*);
  ~CSSPaintImageGeneratorImpl() override;

  // The |container_size| is the container size with subpixel snapping, where
  // the |logical_size| is without it. Both sizes include zoom.
  RefPtr<Image> Paint(const ImageResourceObserver&,
                      const IntSize& container_size,
                      const CSSStyleValueVector*,
                      const LayoutSize* logical_size) final;
  const Vector<CSSPropertyID>& NativeInvalidationProperties() const final;
  const Vector<AtomicString>& CustomInvalidationProperties() const final;
  bool HasAlpha() const final;
  const Vector<CSSSyntaxDescriptor>& InputArgumentTypes() const final;
  bool IsImageGeneratorReady() const final;

  // Should be called from the PaintWorkletGlobalScope when a javascript class
  // is registered with the same name.
  void NotifyGeneratorReady();

  virtual void Trace(blink::Visitor*);

 private:
  CSSPaintImageGeneratorImpl(Observer*, PaintWorklet*, const String&);
  CSSPaintImageGeneratorImpl(PaintWorklet*, const String&);

  bool HasDocumentDefinition() const;

  Member<Observer> observer_;
  Member<PaintWorklet> paint_worklet_;
  const String name_;
};

}  // namespace blink

#endif  // CSSPaintImageGeneratorImpl_h
