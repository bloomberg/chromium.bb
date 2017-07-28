// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/DocumentPaintDefinition.h"
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

  DCHECK(paint_worklet);
  CSSPaintImageGeneratorImpl* generator;
  if (paint_worklet->GetDocumentDefinitionMap().Contains(name)) {
    generator = new CSSPaintImageGeneratorImpl(paint_worklet, name);
  } else {
    generator = new CSSPaintImageGeneratorImpl(observer, paint_worklet, name);
    paint_worklet->AddPendingGenerator(name, generator);
  }

  return generator;
}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(
    PaintWorklet* paint_worklet,
    const String& name)
    : CSSPaintImageGeneratorImpl(nullptr, paint_worklet, name) {}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(
    Observer* observer,
    PaintWorklet* paint_worklet,
    const String& name)
    : observer_(observer), paint_worklet_(paint_worklet), name_(name) {}

CSSPaintImageGeneratorImpl::~CSSPaintImageGeneratorImpl() {}

void CSSPaintImageGeneratorImpl::NotifyGeneratorReady() {
  DCHECK(observer_);
  observer_->PaintImageGeneratorReady();
}

PassRefPtr<Image> CSSPaintImageGeneratorImpl::Paint(
    const ImageResourceObserver& observer,
    const IntSize& size,
    const CSSStyleValueVector* data) {
  return paint_worklet_->Paint(name_, observer, size, data);
}

bool CSSPaintImageGeneratorImpl::HasDocumentDefinition() const {
  return paint_worklet_->GetDocumentDefinitionMap().Contains(name_);
}

const Vector<CSSPropertyID>&
CSSPaintImageGeneratorImpl::NativeInvalidationProperties() const {
  DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, empty_vector, ());
  if (!HasDocumentDefinition())
    return empty_vector;
  DocumentPaintDefinition* definition =
      paint_worklet_->GetDocumentDefinitionMap().at(name_);
  return definition ? definition->NativeInvalidationProperties() : empty_vector;
}

const Vector<AtomicString>&
CSSPaintImageGeneratorImpl::CustomInvalidationProperties() const {
  DEFINE_STATIC_LOCAL(Vector<AtomicString>, empty_vector, ());
  if (!HasDocumentDefinition())
    return empty_vector;
  DocumentPaintDefinition* definition =
      paint_worklet_->GetDocumentDefinitionMap().at(name_);
  return definition ? definition->CustomInvalidationProperties() : empty_vector;
}

bool CSSPaintImageGeneratorImpl::HasAlpha() const {
  if (!HasDocumentDefinition())
    return false;
  DocumentPaintDefinition* definition =
      paint_worklet_->GetDocumentDefinitionMap().at(name_);
  return definition && definition->HasAlpha();
}

const Vector<CSSSyntaxDescriptor>&
CSSPaintImageGeneratorImpl::InputArgumentTypes() const {
  DEFINE_STATIC_LOCAL(Vector<CSSSyntaxDescriptor>, empty_vector, ());
  if (!HasDocumentDefinition())
    return empty_vector;
  DocumentPaintDefinition* definition =
      paint_worklet_->GetDocumentDefinitionMap().at(name_);
  return definition ? definition->InputArgumentTypes() : empty_vector;
}

bool CSSPaintImageGeneratorImpl::IsImageGeneratorReady() const {
  return HasDocumentDefinition();
}

DEFINE_TRACE(CSSPaintImageGeneratorImpl) {
  visitor->Trace(observer_);
  visitor->Trace(paint_worklet_);
  CSSPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
