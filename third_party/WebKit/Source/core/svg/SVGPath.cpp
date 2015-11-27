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

#include "config.h"
#include "core/svg/SVGPath.h"

#include "bindings/core/v8/ExceptionState.h"
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

PassOwnPtr<SVGPathByteStream> blendPathByteStreams(const SVGPathByteStream& fromStream, const SVGPathByteStream& toStream, float progress)
{
    OwnPtr<SVGPathByteStream> resultStream = SVGPathByteStream::create();
    SVGPathByteStreamBuilder builder(*resultStream);
    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource toSource(toStream);
    SVGPathBlender blender(&fromSource, &toSource, &builder);
    blender.blendAnimatedPath(progress);
    return resultStream.release();
}

PassOwnPtr<SVGPathByteStream> addPathByteStreams(PassOwnPtr<SVGPathByteStream> fromStream, const SVGPathByteStream& byStream, unsigned repeatCount = 1)
{
    if (fromStream->isEmpty() || byStream.isEmpty())
        return fromStream;
    OwnPtr<SVGPathByteStream> tempFromStream = fromStream;
    OwnPtr<SVGPathByteStream> resultStream = SVGPathByteStream::create();
    SVGPathByteStreamBuilder builder(*resultStream);
    SVGPathByteStreamSource fromSource(*tempFromStream);
    SVGPathByteStreamSource bySource(byStream);
    SVGPathBlender blender(&fromSource, &bySource, &builder);
    blender.addAnimatedPath(repeatCount);
    return resultStream.release();
}

}

SVGPath::SVGPath()
    : SVGPropertyBase(classType())
{
}

SVGPath::SVGPath(PassOwnPtr<SVGPathByteStream> byteStream)
    : SVGPropertyBase(classType())
    , m_byteStream(byteStream)
{
}

SVGPath::~SVGPath()
{
}

const Path& SVGPath::path() const
{
    if (!m_cachedPath) {
        m_cachedPath = adoptPtr(new Path);
        buildPathFromByteStream(byteStream(), *m_cachedPath);
    }

    return *m_cachedPath;
}

PassRefPtrWillBeRawPtr<SVGPath> SVGPath::clone() const
{
    return SVGPath::create(byteStream().copy());
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGPath::cloneForAnimation(const String& value) const
{
    RefPtrWillBeRawPtr<SVGPath> svgPath = SVGPath::create();
    svgPath->setValueAsString(value, IGNORE_EXCEPTION);
    return svgPath;
}

SVGPathByteStream& SVGPath::ensureByteStream()
{
    if (!m_byteStream)
        m_byteStream = SVGPathByteStream::create();

    return *m_byteStream.get();
}

void SVGPath::byteStreamChanged()
{
    m_cachedPath.clear();
}

const SVGPathByteStream& SVGPath::byteStream() const
{
    return const_cast<SVGPath*>(this)->ensureByteStream();
}

String SVGPath::valueAsString() const
{
    return buildStringFromByteStream(byteStream());
}

void SVGPath::setValueAsString(const String& string, ExceptionState& exceptionState)
{
    if (!buildByteStreamFromString(string, ensureByteStream()))
        exceptionState.throwDOMException(SyntaxError, "Problem parsing path \"" + string + "\"");
    byteStreamChanged();
}

void SVGPath::setValueAsByteStream(PassOwnPtr<SVGPathByteStream> byteStream)
{
    m_byteStream = byteStream;
    byteStreamChanged();
}

void SVGPath::add(PassRefPtrWillBeRawPtr<SVGPropertyBase> other, SVGElement*)
{
    const SVGPathByteStream& otherPathByteStream = toSVGPath(other)->byteStream();
    if (byteStream().size() != otherPathByteStream.size())
        return;

    setValueAsByteStream(addPathByteStreams(m_byteStream.release(), otherPathByteStream));
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

    OwnPtr<SVGPathByteStream> copy;
    if (isToAnimation) {
        copy = byteStream().copy();
        fromStream = copy.get();
    }

    // If the 'from' value is given and it's length doesn't match the 'to' value list length, fallback to a discrete animation.
    if (fromStream->size() != toStream.size() && fromStream->size()) {
        if (percentage < 0.5) {
            if (!isToAnimation) {
                setValueAsByteStream(fromStream->copy());
                return;
            }
        } else {
            setValueAsByteStream(toStream.copy());
            return;
        }
    }

    OwnPtr<SVGPathByteStream> lastAnimatedStream = m_byteStream.release();
    OwnPtr<SVGPathByteStream> newStream = blendPathByteStreams(*fromStream, toStream, percentage);

    // Handle additive='sum'.
    if (!fromStream->size() || (animationElement->isAdditive() && !isToAnimation))
        newStream = addPathByteStreams(newStream.release(), *lastAnimatedStream);

    // Handle accumulate='sum'.
    if (animationElement->isAccumulated() && repeatCount)
        newStream = addPathByteStreams(newStream.release(), toSVGPath(toAtEndOfDurationValue)->byteStream(), repeatCount);

    setValueAsByteStream(newStream.release());
}

float SVGPath::calculateDistance(PassRefPtrWillBeRawPtr<SVGPropertyBase> to, SVGElement*)
{
    // FIXME: Support paced animations.
    return -1;
}

}
