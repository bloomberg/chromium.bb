/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGPath.h"

#include "core/SVGNames.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGPathBlender.h"
#include "core/svg/SVGPathByteStream.h"
#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathUtilities.h"
#include "platform/graphics/Path.h"

namespace blink {

namespace {

PassRefPtr<SVGPathByteStream> blendPathByteStreams(const SVGPathByteStream& fromStream, const SVGPathByteStream& toStream, float progress)
{
    RefPtr<SVGPathByteStream> resultStream = SVGPathByteStream::create();
    SVGPathByteStreamBuilder builder(*resultStream);
    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource toSource(toStream);
    SVGPathBlender blender(&fromSource, &toSource, &builder);
    blender.blendAnimatedPath(progress);
    return resultStream.release();
}

PassRefPtr<SVGPathByteStream> addPathByteStreams(const SVGPathByteStream& fromStream, const SVGPathByteStream& byStream, unsigned repeatCount = 1)
{
    RefPtr<SVGPathByteStream> resultStream = SVGPathByteStream::create();
    SVGPathByteStreamBuilder builder(*resultStream);
    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource bySource(byStream);
    SVGPathBlender blender(&fromSource, &bySource, &builder);
    blender.addAnimatedPath(repeatCount);
    return resultStream.release();
}

PassRefPtr<SVGPathByteStream> conditionallyAddPathByteStreams(PassRefPtr<SVGPathByteStream> fromStream, const SVGPathByteStream& byStream, unsigned repeatCount = 1)
{
    if (fromStream->isEmpty() || byStream.isEmpty())
        return fromStream;
    return addPathByteStreams(*fromStream, byStream, repeatCount);
}

}

SVGPath::SVGPath()
    : SVGPropertyBase(classType())
    , m_pathValue(CSSPathValue::emptyPathValue())
{
}

SVGPath::SVGPath(PassRefPtrWillBeRawPtr<CSSPathValue> pathValue)
    : SVGPropertyBase(classType())
    , m_pathValue(pathValue)
{
}

SVGPath::~SVGPath()
{
}

String SVGPath::valueAsString() const
{
    return m_pathValue->pathString();
}


PassRefPtrWillBeRawPtr<SVGPath> SVGPath::clone() const
{
    return SVGPath::create(m_pathValue);
}

SVGParsingError SVGPath::setValueAsString(const String& string)
{
    SVGParsingError parseStatus = NoError;
    RefPtr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    if (!buildByteStreamFromString(string, *byteStream))
        parseStatus = ParsingAttributeFailedError;
    m_pathValue = CSSPathValue::create(byteStream.release());
    return parseStatus;
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGPath::cloneForAnimation(const String& value) const
{
    return SVGPath::create(CSSPathValue::create(value));
}

void SVGPath::add(PassRefPtrWillBeRawPtr<SVGPropertyBase> other, SVGElement*)
{
    const SVGPathByteStream& otherPathByteStream = toSVGPath(other)->byteStream();
    if (byteStream().size() != otherPathByteStream.size()
        || byteStream().isEmpty()
        || otherPathByteStream.isEmpty())
        return;

    m_pathValue = CSSPathValue::create(addPathByteStreams(byteStream(), otherPathByteStream));
}

void SVGPath::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, PassRefPtrWillBeRawPtr<SVGPropertyBase> fromValue, PassRefPtrWillBeRawPtr<SVGPropertyBase> toValue, PassRefPtrWillBeRawPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*)
{
    ASSERT(animationElement);
    bool isToAnimation = animationElement->animationMode() == ToAnimation;

    const RefPtrWillBeRawPtr<SVGPath> to = toSVGPath(toValue);
    const SVGPathByteStream& toStream = to->byteStream();

    // If no 'to' value is given, nothing to animate.
    if (!toStream.size())
        return;

    const RefPtrWillBeRawPtr<SVGPath> from = toSVGPath(fromValue);
    const SVGPathByteStream* fromStream = &from->byteStream();

    RefPtr<SVGPathByteStream> copy;
    if (isToAnimation) {
        copy = byteStream().clone();
        fromStream = copy.get();
    }

    // If the 'from' value is given and it's length doesn't match the 'to' value list length, fallback to a discrete animation.
    if (fromStream->size() != toStream.size() && fromStream->size()) {
        if (percentage < 0.5) {
            if (!isToAnimation) {
                m_pathValue = from->pathValue();
                return;
            }
        } else {
            m_pathValue = to->pathValue();
            return;
        }
    }

    RefPtr<SVGPathByteStream> newStream = blendPathByteStreams(*fromStream, toStream, percentage);

    // Handle additive='sum'.
    if (animationElement->isAdditive() && !isToAnimation)
        newStream = conditionallyAddPathByteStreams(newStream.release(), byteStream());

    // Handle accumulate='sum'.
    if (animationElement->isAccumulated() && repeatCount)
        newStream = conditionallyAddPathByteStreams(newStream.release(), toSVGPath(toAtEndOfDurationValue)->byteStream(), repeatCount);

    m_pathValue = CSSPathValue::create(newStream.release());
}

float SVGPath::calculateDistance(PassRefPtrWillBeRawPtr<SVGPropertyBase> to, SVGElement*)
{
    // FIXME: Support paced animations.
    return -1;
}

DEFINE_TRACE(SVGPath)
{
    visitor->trace(m_pathValue);
    SVGPropertyBase::trace(visitor);
}

}
