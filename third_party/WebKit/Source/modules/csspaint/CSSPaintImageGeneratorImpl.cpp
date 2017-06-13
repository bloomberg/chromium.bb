// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorklet.h"
#include "modules/csspaint/WindowPaintWorklet.h"
#include "platform/graphics/Image.h"

namespace blink {

CSSPaintImageGenerator* CSSPaintImageGeneratorImpl::Create(
    const String& name,
    const Document& document,
    Observer* observer) {
  LocalDOMWindow* dom_window = document.domWindow();
  PaintWorklet* paint_worklet =
      WindowPaintWorklet::From(*dom_window).paintWorklet();

  CSSPaintDefinition* paint_definition = paint_worklet->FindDefinition(name);
  CSSPaintImageGeneratorImpl* generator;
  if (!paint_definition) {
    generator = new CSSPaintImageGeneratorImpl(observer);
    paint_worklet->AddPendingGenerator(name, generator);
  } else {
    generator = new CSSPaintImageGeneratorImpl(paint_definition);
  }

  return generator;
}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(
    CSSPaintDefinition* definition)
    : definition_(definition) {}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(Observer* observer)
    : observer_(observer) {}

CSSPaintImageGeneratorImpl::~CSSPaintImageGeneratorImpl() {}

void CSSPaintImageGeneratorImpl::SetDefinition(CSSPaintDefinition* definition) {
  DCHECK(!definition_);
  definition_ = definition;

  DCHECK(observer_);
  observer_->PaintImageGeneratorReady();
}

PassRefPtr<Image> CSSPaintImageGeneratorImpl::Paint(
    const ImageResourceObserver& observer,
    const IntSize& size,
    const CSSStyleValueVector* data) {
  return definition_ ? definition_->Paint(observer, size, data) : nullptr;
}

const Vector<CSSPropertyID>&
CSSPaintImageGeneratorImpl::NativeInvalidationProperties() const {
  DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, empty_vector, ());
  return definition_ ? definition_->NativeInvalidationProperties()
                     : empty_vector;
}

const Vector<AtomicString>&
CSSPaintImageGeneratorImpl::CustomInvalidationProperties() const {
  DEFINE_STATIC_LOCAL(Vector<AtomicString>, empty_vector, ());
  return definition_ ? definition_->CustomInvalidationProperties()
                     : empty_vector;
}

bool CSSPaintImageGeneratorImpl::HasAlpha() const {
  return definition_ && definition_->HasAlpha();
}

const Vector<CSSSyntaxDescriptor>&
CSSPaintImageGeneratorImpl::InputArgumentTypes() const {
  DEFINE_STATIC_LOCAL(Vector<CSSSyntaxDescriptor>, empty_vector, ());
  return definition_ ? definition_->InputArgumentTypes() : empty_vector;
}

bool CSSPaintImageGeneratorImpl::IsImageGeneratorReady() const {
  return definition_;
}

DEFINE_TRACE(CSSPaintImageGeneratorImpl) {
  visitor->Trace(definition_);
  visitor->Trace(observer_);
  CSSPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
