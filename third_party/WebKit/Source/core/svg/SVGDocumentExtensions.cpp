/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/svg/SVGDocumentExtensions.h"

#include "core/dom/Document.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/animation/SMILTimeContainer.h"
#include "wtf/AutoReset.h"

namespace blink {

SVGDocumentExtensions::SVGDocumentExtensions(Document* document)
    : m_document(document)
{
}

SVGDocumentExtensions::~SVGDocumentExtensions() {}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement* element) {
  m_timeContainers.insert(element);
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement* element) {
  m_timeContainers.erase(element);
}

void SVGDocumentExtensions::addWebAnimationsPendingSVGElement(
    SVGElement& element) {
  ASSERT(RuntimeEnabledFeatures::webAnimationsSVGEnabled());
  m_webAnimationsPendingSVGElements.insert(&element);
}

void SVGDocumentExtensions::serviceOnAnimationFrame(Document& document) {
  if (!document.svgExtensions())
    return;
  document.accessSVGExtensions().serviceAnimations();
}

void SVGDocumentExtensions::serviceAnimations() {
  if (RuntimeEnabledFeatures::smilEnabled()) {
    HeapVector<Member<SVGSVGElement>> timeContainers;
    copyToVector(m_timeContainers, timeContainers);
    for (const auto& container : timeContainers)
      container->timeContainer()->serviceAnimations();
  }

  SVGElementSet webAnimationsPendingSVGElements;
  webAnimationsPendingSVGElements.swap(m_webAnimationsPendingSVGElements);

  // TODO(alancutter): Make SVG animation effect application a separate document
  // lifecycle phase from servicing animations to be responsive to Javascript
  // manipulation of exposed animation objects.
  for (auto& svgElement : webAnimationsPendingSVGElements)
    svgElement->applyActiveWebAnimations();

  ASSERT(m_webAnimationsPendingSVGElements.isEmpty());
}

void SVGDocumentExtensions::startAnimations() {
  // FIXME: Eventually every "Time Container" will need a way to latch on to
  // some global timer starting animations for a document will do this
  // "latching"
  // FIXME: We hold a ref pointers to prevent a shadow tree from getting removed
  // out from underneath us.  In the future we should refactor the use-element
  // to avoid this. See https://webkit.org/b/53704
  HeapVector<Member<SVGSVGElement>> timeContainers;
  copyToVector(m_timeContainers, timeContainers);
  for (const auto& container : timeContainers) {
    SMILTimeContainer* timeContainer = container->timeContainer();
    if (!timeContainer->isStarted())
      timeContainer->start();
  }
}

void SVGDocumentExtensions::pauseAnimations() {
  for (SVGSVGElement* element : m_timeContainers)
    element->pauseAnimations();
}

void SVGDocumentExtensions::dispatchSVGLoadEventToOutermostSVGElements() {
  HeapVector<Member<SVGSVGElement>> timeContainers;
  copyToVector(m_timeContainers, timeContainers);
  for (const auto& container : timeContainers) {
    SVGSVGElement* outerSVG = container.get();
    if (!outerSVG->isOutermostSVGSVGElement())
      continue;

    // Don't dispatch the load event document is not wellformed (for
    // XML/standalone svg).
    if (outerSVG->document().wellFormed() ||
        !outerSVG->document().isSVGDocument())
      outerSVG->sendSVGLoadEventIfPossible();
  }
}

void SVGDocumentExtensions::reportError(const String& message) {
  ConsoleMessage* consoleMessage = ConsoleMessage::create(
      RenderingMessageSource, ErrorMessageLevel, "Error: " + message);
  m_document->addConsoleMessage(consoleMessage);
}

void SVGDocumentExtensions::addSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svgRoot) {
  ASSERT(!m_inRelativeLengthSVGRootsInvalidation);
  m_relativeLengthSVGRoots.insert(svgRoot);
}

void SVGDocumentExtensions::removeSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svgRoot) {
  ASSERT(!m_inRelativeLengthSVGRootsInvalidation);
  m_relativeLengthSVGRoots.erase(svgRoot);
}

bool SVGDocumentExtensions::isSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svgRoot) const {
  return m_relativeLengthSVGRoots.contains(svgRoot);
}

void SVGDocumentExtensions::invalidateSVGRootsWithRelativeLengthDescendents(
    SubtreeLayoutScope* scope) {
  ASSERT(!m_inRelativeLengthSVGRootsInvalidation);
#if DCHECK_IS_ON()
  AutoReset<bool> inRelativeLengthSVGRootsChange(
      &m_inRelativeLengthSVGRootsInvalidation, true);
#endif

  for (SVGSVGElement* element : m_relativeLengthSVGRoots)
    element->invalidateRelativeLengthClients(scope);
}

bool SVGDocumentExtensions::zoomAndPanEnabled() const {
  if (SVGSVGElement* svg = rootElement(*m_document))
    return svg->zoomAndPanEnabled();
  return false;
}

void SVGDocumentExtensions::startPan(const FloatPoint& start) {
  if (SVGSVGElement* svg = rootElement(*m_document))
    m_translate = FloatPoint(start.x() - svg->currentTranslate().x(),
                             start.y() - svg->currentTranslate().y());
}

void SVGDocumentExtensions::updatePan(const FloatPoint& pos) const {
  if (SVGSVGElement* svg = rootElement(*m_document))
    svg->setCurrentTranslate(
        FloatPoint(pos.x() - m_translate.x(), pos.y() - m_translate.y()));
}

SVGSVGElement* SVGDocumentExtensions::rootElement(const Document& document) {
  Element* elem = document.documentElement();
  return isSVGSVGElement(elem) ? toSVGSVGElement(elem) : 0;
}

SVGSVGElement* SVGDocumentExtensions::rootElement() const {
  ASSERT(m_document);
  return rootElement(*m_document);
}

DEFINE_TRACE(SVGDocumentExtensions) {
  visitor->trace(m_document);
  visitor->trace(m_timeContainers);
  visitor->trace(m_webAnimationsPendingSVGElements);
  visitor->trace(m_relativeLengthSVGRoots);
}

}  // namespace blink
