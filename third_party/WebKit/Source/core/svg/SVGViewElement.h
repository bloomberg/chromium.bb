/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGViewElement_h
#define SVGViewElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGAnimatedRect.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGStaticStringList.h"
#include "core/svg/SVGZoomAndPan.h"

namespace WebCore {

class SVGViewElement FINAL : public SVGElement,
                             public SVGFitToViewBox,
                             public SVGZoomAndPan {
public:
    static PassRefPtr<SVGViewElement> create(Document&);

    using SVGElement::ref;
    using SVGElement::deref;

    SVGAnimatedRect* viewBox() { return m_viewBox.get(); }

    SVGStringListTearOff* viewTarget() { return m_viewTarget->tearOff(); }
    SVGZoomAndPanType zoomAndPan() const { return m_zoomAndPan; }
    void setZoomAndPan(unsigned short zoomAndPan) { m_zoomAndPan = SVGZoomAndPan::parseFromNumber(zoomAndPan); }
    SVGAnimatedPreserveAspectRatio* preserveAspectRatio() { return m_preserveAspectRatio.get(); }

private:
    explicit SVGViewElement(Document&);

    // FIXME: svgAttributeChanged missing.
    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }

    RefPtr<SVGAnimatedRect> m_viewBox;
    RefPtr<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;
    RefPtr<SVGStaticStringList> m_viewTarget;
    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGViewElement)
    END_DECLARE_ANIMATED_PROPERTIES

    SVGZoomAndPanType m_zoomAndPan;
};

DEFINE_NODE_TYPE_CASTS(SVGViewElement, hasTagName(SVGNames::viewTag));

} // namespace WebCore

#endif
