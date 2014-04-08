/*
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

#ifndef SVGViewSpec_h
#define SVGViewSpec_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGZoomAndPan.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class SVGViewSpec FINAL : public RefCounted<SVGViewSpec>, public ScriptWrappable, public SVGZoomAndPan, public SVGFitToViewBox {
public:
    using RefCounted<SVGViewSpec>::ref;
    using RefCounted<SVGViewSpec>::deref;

    static PassRefPtr<SVGViewSpec> create(SVGSVGElement* contextElement)
    {
        return adoptRef(new SVGViewSpec(contextElement));
    }

    bool parseViewSpec(const String&);
    void reset();
    void detachContextElement();

    // JS API
    SVGTransformList* transform() { return m_transform ? m_transform->baseValue() : 0; }
    PassRefPtr<SVGTransformListTearOff> transformFromJavascript() { return m_transform ? m_transform->baseVal() : 0; }
    SVGElement* viewTarget() const;
    String viewBoxString() const;
    String preserveAspectRatioString() const;
    String transformString() const;
    String viewTargetString() const { return m_viewTargetString; }
    // override SVGZoomAndPan.setZoomAndPan so can throw exception on write
    void setZoomAndPan(unsigned short value) { } // read only
    void setZoomAndPan(unsigned short value, ExceptionState&);

private:
    explicit SVGViewSpec(SVGSVGElement*);

    template<typename CharType>
    bool parseViewSpecInternal(const CharType* ptr, const CharType* end);

    // FIXME(oilpan): This is back-ptr to be cleared from contextElement.
    SVGSVGElement* m_contextElement;
    RefPtr<SVGAnimatedTransformList> m_transform;
    String m_viewTargetString;
};

} // namespace WebCore

#endif
