// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGElementRareData.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/svg/SVGElementProxy.h"

namespace blink {

MutableStylePropertySet*
SVGElementRareData::ensureAnimatedSMILStyleProperties() {
  if (!m_animatedSMILStyleProperties)
    m_animatedSMILStyleProperties =
        MutableStylePropertySet::create(SVGAttributeMode);
  return m_animatedSMILStyleProperties.get();
}

ComputedStyle* SVGElementRareData::overrideComputedStyle(
    Element* element,
    const ComputedStyle* parentStyle) {
  ASSERT(element);
  if (!m_useOverrideComputedStyle)
    return nullptr;
  if (!m_overrideComputedStyle || m_needsOverrideComputedStyleUpdate) {
    // The style computed here contains no CSS Animations/Transitions or SMIL
    // induced rules - this is needed to compute the "base value" for the SMIL
    // animation sandwhich model.
    m_overrideComputedStyle =
        element->document().ensureStyleResolver().styleForElement(
            element, parentStyle, parentStyle, DisallowStyleSharing,
            MatchAllRulesExcludingSMIL);
    m_needsOverrideComputedStyleUpdate = false;
  }
  ASSERT(m_overrideComputedStyle);
  return m_overrideComputedStyle.get();
}

DEFINE_TRACE(SVGElementRareData) {
  visitor->trace(m_outgoingReferences);
  visitor->trace(m_incomingReferences);
  visitor->trace(m_elementProxySet);
  visitor->trace(m_animatedSMILStyleProperties);
  visitor->trace(m_elementInstances);
  visitor->trace(m_correspondingElement);
  visitor->trace(m_owner);
}

AffineTransform* SVGElementRareData::animateMotionTransform() {
  return &m_animateMotionTransform;
}

SVGElementProxySet& SVGElementRareData::ensureElementProxySet() {
  if (!m_elementProxySet)
    m_elementProxySet = new SVGElementProxySet;
  return *m_elementProxySet;
}

}  // namespace blink
