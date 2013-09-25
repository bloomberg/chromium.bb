/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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
#include "core/svg/SVGPreserveAspectRatio.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/transforms/AffineTransform.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

SVGPreserveAspectRatio::SVGPreserveAspectRatio()
    : m_align(SVG_PRESERVEASPECTRATIO_XMIDYMID)
    , m_meetOrSlice(SVG_MEETORSLICE_MEET)
{
}

void SVGPreserveAspectRatio::setAlign(unsigned short align, ExceptionState& es)
{
    if (align == SVG_PRESERVEASPECTRATIO_UNKNOWN || align > SVG_PRESERVEASPECTRATIO_XMAXYMAX) {
        es.throwUninformativeAndGenericDOMException(NotSupportedError);
        return;
    }

    m_align = static_cast<SVGPreserveAspectRatioType>(align);
}

void SVGPreserveAspectRatio::setMeetOrSlice(unsigned short meetOrSlice, ExceptionState& es)
{
    if (meetOrSlice == SVG_MEETORSLICE_UNKNOWN || meetOrSlice > SVG_MEETORSLICE_SLICE) {
        es.throwUninformativeAndGenericDOMException(NotSupportedError);
        return;
    }

    m_meetOrSlice = static_cast<SVGMeetOrSliceType>(meetOrSlice);
}

template<typename CharType>
bool SVGPreserveAspectRatio::parseInternal(const CharType*& ptr, const CharType* end, bool validate)
{
    // FIXME: Rewrite this parser, without gotos!
    if (!skipOptionalSVGSpaces(ptr, end))
        goto bailOut;

    if (*ptr == 'd') {
        if (!skipString(ptr, end, "defer"))
            goto bailOut;

        // FIXME: We just ignore the "defer" here.
        if (ptr == end)
            return true;

        if (!skipOptionalSVGSpaces(ptr, end))
            goto bailOut;
    }

    if (*ptr == 'n') {
        if (!skipString(ptr, end, "none"))
            goto bailOut;
        m_align = SVG_PRESERVEASPECTRATIO_NONE;
        skipOptionalSVGSpaces(ptr, end);
    } else if (*ptr == 'x') {
        if ((end - ptr) < 8)
            goto bailOut;
        if (ptr[1] != 'M' || ptr[4] != 'Y' || ptr[5] != 'M')
            goto bailOut;
        if (ptr[2] == 'i') {
            if (ptr[3] == 'n') {
                if (ptr[6] == 'i') {
                    if (ptr[7] == 'n')
                        m_align = SVG_PRESERVEASPECTRATIO_XMINYMIN;
                    else if (ptr[7] == 'd')
                        m_align = SVG_PRESERVEASPECTRATIO_XMINYMID;
                    else
                        goto bailOut;
                } else if (ptr[6] == 'a' && ptr[7] == 'x')
                     m_align = SVG_PRESERVEASPECTRATIO_XMINYMAX;
                else
                     goto bailOut;
            } else if (ptr[3] == 'd') {
                if (ptr[6] == 'i') {
                    if (ptr[7] == 'n')
                        m_align = SVG_PRESERVEASPECTRATIO_XMIDYMIN;
                    else if (ptr[7] == 'd')
                        m_align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
                    else
                        goto bailOut;
                } else if (ptr[6] == 'a' && ptr[7] == 'x')
                    m_align = SVG_PRESERVEASPECTRATIO_XMIDYMAX;
                else
                    goto bailOut;
            } else
                goto bailOut;
        } else if (ptr[2] == 'a' && ptr[3] == 'x') {
            if (ptr[6] == 'i') {
                if (ptr[7] == 'n')
                    m_align = SVG_PRESERVEASPECTRATIO_XMAXYMIN;
                else if (ptr[7] == 'd')
                    m_align = SVG_PRESERVEASPECTRATIO_XMAXYMID;
                else
                    goto bailOut;
            } else if (ptr[6] == 'a' && ptr[7] == 'x')
                m_align = SVG_PRESERVEASPECTRATIO_XMAXYMAX;
            else
                goto bailOut;
        } else
            goto bailOut;
        ptr += 8;
        skipOptionalSVGSpaces(ptr, end);
    } else
        goto bailOut;

    if (ptr < end) {
        if (*ptr == 'm') {
            if (!skipString(ptr, end, "meet"))
                goto bailOut;
            skipOptionalSVGSpaces(ptr, end);
        } else if (*ptr == 's') {
            if (!skipString(ptr, end, "slice"))
                goto bailOut;
            skipOptionalSVGSpaces(ptr, end);
            if (m_align != SVG_PRESERVEASPECTRATIO_NONE)
                m_meetOrSlice = SVG_MEETORSLICE_SLICE;
        }
    }

    if (end != ptr && validate) {
bailOut:
        m_align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
        m_meetOrSlice = SVG_MEETORSLICE_MEET;
        return false;
    }
    return true;
}

void SVGPreserveAspectRatio::parse(const String& string)
{
    if (string.isEmpty()) {
        const LChar* ptr = 0;
        parseInternal(ptr, ptr, true);
    } else if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        parseInternal(ptr, end, true);
    } else {
        const UChar* ptr = string.characters16();
        const UChar* end = ptr + string.length();
        parseInternal(ptr, end, true);
    }
}

bool SVGPreserveAspectRatio::parse(const LChar*& ptr, const LChar* end, bool validate)
{
    return parseInternal(ptr, end, validate);
}

bool SVGPreserveAspectRatio::parse(const UChar*& ptr, const UChar* end, bool validate)
{
    return parseInternal(ptr, end, validate);
}

void SVGPreserveAspectRatio::transformRect(FloatRect& destRect, FloatRect& srcRect)
{
    if (m_align == SVG_PRESERVEASPECTRATIO_NONE)
        return;

    FloatSize imageSize = srcRect.size();
    float origDestWidth = destRect.width();
    float origDestHeight = destRect.height();
    switch (m_meetOrSlice) {
    case SVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN:
        break;
    case SVGPreserveAspectRatio::SVG_MEETORSLICE_MEET: {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        if (origDestHeight > origDestWidth * widthToHeightMultiplier) {
            destRect.setHeight(origDestWidth * widthToHeightMultiplier);
            switch (m_align) {
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                destRect.setY(destRect.y() + origDestHeight / 2 - destRect.height() / 2);
                break;
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                destRect.setY(destRect.y() + origDestHeight - destRect.height());
                break;
            default:
                break;
            }
        }
        if (origDestWidth > origDestHeight / widthToHeightMultiplier) {
            destRect.setWidth(origDestHeight / widthToHeightMultiplier);
            switch (m_align) {
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                destRect.setX(destRect.x() + origDestWidth / 2 - destRect.width() / 2);
                break;
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                destRect.setX(destRect.x() + origDestWidth - destRect.width());
                break;
            default:
                break;
            }
        }
        break;
    }
    case SVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE: {
        float widthToHeightMultiplier = srcRect.height() / srcRect.width();
        // if the destination height is less than the height of the image we'll be drawing
        if (origDestHeight < origDestWidth * widthToHeightMultiplier) {
            float destToSrcMultiplier = srcRect.width() / destRect.width();
            srcRect.setHeight(destRect.height() * destToSrcMultiplier);
            switch (m_align) {
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
                srcRect.setY(srcRect.y() + imageSize.height() / 2 - srcRect.height() / 2);
                break;
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                srcRect.setY(srcRect.y() + imageSize.height() - srcRect.height());
                break;
            default:
                break;
            }
        }
        // if the destination width is less than the width of the image we'll be drawing
        if (origDestWidth < origDestHeight / widthToHeightMultiplier) {
            float destToSrcMultiplier = srcRect.height() / destRect.height();
            srcRect.setWidth(destRect.width() * destToSrcMultiplier);
            switch (m_align) {
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
                srcRect.setX(srcRect.x() + imageSize.width() / 2 - srcRect.width() / 2);
                break;
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
            case SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
                srcRect.setX(srcRect.x() + imageSize.width() - srcRect.width());
                break;
            default:
                break;
            }
        }
        break;
    }
    }
}

AffineTransform SVGPreserveAspectRatio::getCTM(float logicalX, float logicalY, float logicalWidth, float logicalHeight, float physicalWidth, float physicalHeight) const
{
    AffineTransform transform;
    if (m_align == SVG_PRESERVEASPECTRATIO_UNKNOWN)
        return transform;

    double extendedLogicalX = logicalX;
    double extendedLogicalY = logicalY;
    double extendedLogicalWidth = logicalWidth;
    double extendedLogicalHeight = logicalHeight;
    double extendedPhysicalWidth = physicalWidth;
    double extendedPhysicalHeight = physicalHeight;
    double logicalRatio = extendedLogicalWidth / extendedLogicalHeight;
    double physicalRatio = extendedPhysicalWidth / extendedPhysicalHeight;

    if (m_align == SVG_PRESERVEASPECTRATIO_NONE) {
        transform.scaleNonUniform(extendedPhysicalWidth / extendedLogicalWidth, extendedPhysicalHeight / extendedLogicalHeight);
        transform.translate(-extendedLogicalX, -extendedLogicalY);
        return transform;
    }

    if ((logicalRatio < physicalRatio && (m_meetOrSlice == SVG_MEETORSLICE_MEET)) || (logicalRatio >= physicalRatio && (m_meetOrSlice == SVG_MEETORSLICE_SLICE))) {
        transform.scaleNonUniform(extendedPhysicalHeight / extendedLogicalHeight, extendedPhysicalHeight / extendedLogicalHeight);

        if (m_align == SVG_PRESERVEASPECTRATIO_XMINYMIN || m_align == SVG_PRESERVEASPECTRATIO_XMINYMID || m_align == SVG_PRESERVEASPECTRATIO_XMINYMAX)
            transform.translate(-extendedLogicalX, -extendedLogicalY);
        else if (m_align == SVG_PRESERVEASPECTRATIO_XMIDYMIN || m_align == SVG_PRESERVEASPECTRATIO_XMIDYMID || m_align == SVG_PRESERVEASPECTRATIO_XMIDYMAX)
            transform.translate(-extendedLogicalX - (extendedLogicalWidth - extendedPhysicalWidth * extendedLogicalHeight / extendedPhysicalHeight) / 2, -extendedLogicalY);
        else
            transform.translate(-extendedLogicalX - (extendedLogicalWidth - extendedPhysicalWidth * extendedLogicalHeight / extendedPhysicalHeight), -extendedLogicalY);

        return transform;
    }

    transform.scaleNonUniform(extendedPhysicalWidth / extendedLogicalWidth, extendedPhysicalWidth / extendedLogicalWidth);

    if (m_align == SVG_PRESERVEASPECTRATIO_XMINYMIN || m_align == SVG_PRESERVEASPECTRATIO_XMIDYMIN || m_align == SVG_PRESERVEASPECTRATIO_XMAXYMIN)
        transform.translate(-extendedLogicalX, -extendedLogicalY);
    else if (m_align == SVG_PRESERVEASPECTRATIO_XMINYMID || m_align == SVG_PRESERVEASPECTRATIO_XMIDYMID || m_align == SVG_PRESERVEASPECTRATIO_XMAXYMID)
        transform.translate(-extendedLogicalX, -extendedLogicalY - (extendedLogicalHeight - extendedPhysicalHeight * extendedLogicalWidth / extendedPhysicalWidth) / 2);
    else
        transform.translate(-extendedLogicalX, -extendedLogicalY - (extendedLogicalHeight - extendedPhysicalHeight * extendedLogicalWidth / extendedPhysicalWidth));

    return transform;
}

String SVGPreserveAspectRatio::valueAsString() const
{
    String alignType;

    switch (m_align) {
    case SVG_PRESERVEASPECTRATIO_NONE:
        alignType = "none";
        break;
    case SVG_PRESERVEASPECTRATIO_XMINYMIN:
        alignType = "xMinYMin";
        break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
        alignType = "xMidYMin";
        break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
        alignType = "xMaxYMin";
        break;
    case SVG_PRESERVEASPECTRATIO_XMINYMID:
        alignType = "xMinYMid";
        break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMID:
        alignType = "xMidYMid";
        break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMID:
        alignType = "xMaxYMid";
        break;
    case SVG_PRESERVEASPECTRATIO_XMINYMAX:
        alignType = "xMinYMax";
        break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
        alignType = "xMidYMax";
        break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        alignType = "xMaxYMax";
        break;
    case SVG_PRESERVEASPECTRATIO_UNKNOWN:
        alignType = "unknown";
        break;
    };

    switch (m_meetOrSlice) {
    default:
    case SVG_MEETORSLICE_UNKNOWN:
        return alignType;
    case SVG_MEETORSLICE_MEET:
        return alignType + " meet";
    case SVG_MEETORSLICE_SLICE:
        return alignType + " slice";
    }
}

}
