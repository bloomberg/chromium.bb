/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "config.h"
#include "core/svg/SVGDocument.h"

#include "SVGNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/events/EventNames.h"
#include "core/page/FrameView.h"
#include "core/rendering/RenderView.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGViewSpec.h"
#include "core/svg/SVGZoomAndPan.h"
#include "core/svg/SVGZoomEvent.h"

namespace WebCore {

SVGDocument::SVGDocument(const DocumentInit& initializer)
    : Document(initializer, SVGDocumentClass)
{
    ScriptWrappable::init(this);
}

SVGSVGElement* SVGDocument::rootElement() const
{
    Element* elem = documentElement();
    if (elem && elem->hasTagName(SVGNames::svgTag))
        return toSVGSVGElement(elem);

    return 0;
}

void SVGDocument::dispatchZoomEvent(float prevScale, float newScale)
{
    RefPtr<SVGZoomEvent> event = static_pointer_cast<SVGZoomEvent>(createEvent("SVGZoomEvents", IGNORE_EXCEPTION));
    event->initEvent(eventNames().zoomEvent, true, false);
    event->setPreviousScale(prevScale);
    event->setNewScale(newScale);
    rootElement()->dispatchEvent(event.release(), IGNORE_EXCEPTION);
}

void SVGDocument::dispatchScrollEvent()
{
    RefPtr<Event> event = createEvent("SVGEvents", IGNORE_EXCEPTION);
    event->initEvent(eventNames().scrollEvent, true, false);
    rootElement()->dispatchEvent(event.release(), IGNORE_EXCEPTION);
}

bool SVGDocument::zoomAndPanEnabled() const
{
    if (SVGSVGElement* svg = rootElement()) {
        if (svg->useCurrentView()) {
            if (svg->currentView())
                return svg->currentView()->zoomAndPan() == SVGZoomAndPanMagnify;
        } else {
            return svg->zoomAndPan() == SVGZoomAndPanMagnify;
        }
    }

    return false;
}

void SVGDocument::startPan(const FloatPoint& start)
{
    if (SVGSVGElement* svg = rootElement())
        m_translate = FloatPoint(start.x() - svg->currentTranslate().x(), start.y() - svg->currentTranslate().y());
}

void SVGDocument::updatePan(const FloatPoint& pos) const
{
    if (SVGSVGElement* svg = rootElement()) {
        svg->setCurrentTranslate(FloatPoint(pos.x() - m_translate.x(), pos.y() - m_translate.y()));
        if (renderer())
            renderer()->repaint();
    }
}

bool SVGDocument::childShouldCreateRenderer(const Node& child) const
{
    if (child.hasTagName(SVGNames::svgTag))
        return toSVGSVGElement(&child)->isValid();
    return true;
}

}
