/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
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

#ifndef SVGFitToViewBox_h
#define SVGFitToViewBox_h

#include "SVGNames.h"
#include "core/dom/QualifiedName.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGRect.h"
#include "wtf/HashSet.h"

namespace WebCore {

class AffineTransform;
class Document;

class SVGFitToViewBox {
public:
    static AffineTransform viewBoxToViewTransform(const FloatRect& viewBoxRect, PassRefPtr<SVGPreserveAspectRatio>, float viewWidth, float viewHeight);

    static bool isKnownAttribute(const QualifiedName&);
    static void addSupportedAttributes(HashSet<QualifiedName>&);

    template<class SVGElementTarget>
    static bool parseAttribute(SVGElementTarget* target, const QualifiedName& name, const AtomicString& value)
    {
        ASSERT(target);

        SVGParsingError parseError = NoError;

        if (name == SVGNames::viewBoxAttr) {
            target->viewBox()->setBaseValueAsString(value, parseError);
            if (target->viewBox()->baseValue()->width() < 0.0f) {
                target->document().accessSVGExtensions()->reportError("A negative value for ViewBox width is not allowed");
                target->viewBox()->baseValue()->setInvalid();
            }
            if (target->viewBox()->baseValue()->height() < 0.0f) {
                target->document().accessSVGExtensions()->reportError("A negative value for ViewBox height is not allowed");
                target->viewBox()->baseValue()->setInvalid();
            }
        } else if (name == SVGNames::preserveAspectRatioAttr) {
            target->preserveAspectRatio()->setBaseValueAsString(value, parseError);
        } else {
            return false;
        }

        target->reportAttributeParsingError(parseError, name, value);
        return true;
    }
};

} // namespace WebCore

#endif // SVGFitToViewBox_h
