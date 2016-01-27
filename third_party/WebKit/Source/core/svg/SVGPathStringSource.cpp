/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "core/svg/SVGPathStringSource.h"

#include "core/svg/SVGParserUtilities.h"
#include "platform/geometry/FloatPoint.h"

namespace blink {

SVGPathStringSource::SVGPathStringSource(const String& string)
    : m_is8BitSource(string.is8Bit())
    , m_previousCommand(PathSegUnknown)
    , m_string(string)
{
    ASSERT(!string.isNull());

    if (m_is8BitSource) {
        m_current.m_character8 = string.characters8();
        m_end.m_character8 = m_current.m_character8 + string.length();
    } else {
        m_current.m_character16 = string.characters16();
        m_end.m_character16 = m_current.m_character16 + string.length();
    }
    eatWhitespace();
}

bool SVGPathStringSource::hasMoreData() const
{
    if (m_is8BitSource)
        return m_current.m_character8 < m_end.m_character8;
    return m_current.m_character16 < m_end.m_character16;
}

void SVGPathStringSource::eatWhitespace()
{
    if (m_is8BitSource)
        skipOptionalSVGSpaces(m_current.m_character8, m_end.m_character8);
    else
        skipOptionalSVGSpaces(m_current.m_character16, m_end.m_character16);
}

static SVGPathSegType parseSVGSegmentTypeHelper(unsigned lookahead)
{
    switch (lookahead) {
    case 'Z':
    case 'z':
        return PathSegClosePath;
    case 'M':
        return PathSegMoveToAbs;
    case 'm':
        return PathSegMoveToRel;
    case 'L':
        return PathSegLineToAbs;
    case 'l':
        return PathSegLineToRel;
    case 'C':
        return PathSegCurveToCubicAbs;
    case 'c':
        return PathSegCurveToCubicRel;
    case 'Q':
        return PathSegCurveToQuadraticAbs;
    case 'q':
        return PathSegCurveToQuadraticRel;
    case 'A':
        return PathSegArcAbs;
    case 'a':
        return PathSegArcRel;
    case 'H':
        return PathSegLineToHorizontalAbs;
    case 'h':
        return PathSegLineToHorizontalRel;
    case 'V':
        return PathSegLineToVerticalAbs;
    case 'v':
        return PathSegLineToVerticalRel;
    case 'S':
        return PathSegCurveToCubicSmoothAbs;
    case 's':
        return PathSegCurveToCubicSmoothRel;
    case 'T':
        return PathSegCurveToQuadraticSmoothAbs;
    case 't':
        return PathSegCurveToQuadraticSmoothRel;
    default:
        return PathSegUnknown;
    }
}

static bool nextCommandHelper(unsigned lookahead, SVGPathSegType previousCommand, SVGPathSegType& nextCommand)
{
    // Check for remaining coordinates in the current command.
    if ((lookahead == '+' || lookahead == '-' || lookahead == '.' || (lookahead >= '0' && lookahead <= '9'))
        && previousCommand != PathSegClosePath) {
        if (previousCommand == PathSegMoveToAbs) {
            nextCommand = PathSegLineToAbs;
            return true;
        }
        if (previousCommand == PathSegMoveToRel) {
            nextCommand = PathSegLineToRel;
            return true;
        }
        nextCommand = previousCommand;
        return true;
    }
    return false;
}

void SVGPathStringSource::setErrorMark(SVGParseStatus status)
{
    if (m_error.status() != SVGParseStatus::NoError)
        return;
    size_t locus = m_is8BitSource
        ? m_current.m_character8 - m_string.characters8()
        : m_current.m_character16 - m_string.characters16();
    m_error = SVGParsingError(status, locus);
}

float SVGPathStringSource::parseNumberWithError()
{
    float numberValue = 0;
    bool error;
    if (m_is8BitSource)
        error = !parseNumber(m_current.m_character8, m_end.m_character8, numberValue);
    else
        error = !parseNumber(m_current.m_character16, m_end.m_character16, numberValue);
    if (UNLIKELY(error))
        setErrorMark(SVGParseStatus::ExpectedNumber);
    return numberValue;
}

bool SVGPathStringSource::parseArcFlagWithError()
{
    bool flagValue = false;
    bool error;
    if (m_is8BitSource)
        error = !parseArcFlag(m_current.m_character8, m_end.m_character8, flagValue);
    else
        error = !parseArcFlag(m_current.m_character16, m_end.m_character16, flagValue);
    if (UNLIKELY(error))
        setErrorMark(SVGParseStatus::ExpectedArcFlag);
    return flagValue;
}

SVGPathSegType SVGPathStringSource::peekSegmentType()
{
    ASSERT(hasMoreData());
    // This won't work in all cases because of the state required to "detect" implicit commands.
    unsigned lookahead = m_is8BitSource ? *m_current.m_character8 : *m_current.m_character16;
    SVGPathSegType segmentType = parseSVGSegmentTypeHelper(lookahead);
    // Here we assume that this is only called via SVGPathParser::initialCommandIsMoveTo.
    // TODO(fs): It ought to be possible to refactor away this entirely, and
    // just handle this in parseSegment (which we sort of do already...) The
    // only user is the method mentioned above.
    if (segmentType != PathSegMoveToAbs && segmentType != PathSegMoveToRel)
        setErrorMark(SVGParseStatus::ExpectedMoveToCommand);
    return segmentType;
}

PathSegmentData SVGPathStringSource::parseSegment()
{
    ASSERT(hasMoreData());
    PathSegmentData segment;
    unsigned lookahead = m_is8BitSource ? *m_current.m_character8 : *m_current.m_character16;
    SVGPathSegType command = parseSVGSegmentTypeHelper(lookahead);
    if (command == PathSegUnknown) {
        // Possibly an implicit command. Not allowed if this is the first command.
        if (m_previousCommand == PathSegUnknown) {
            setErrorMark(SVGParseStatus::ExpectedMoveToCommand);
            return segment;
        }
        if (!nextCommandHelper(lookahead, m_previousCommand, command)) {
            setErrorMark(SVGParseStatus::ExpectedPathCommand);
            return segment;
        }
    } else {
        // Valid explicit command.
        if (m_is8BitSource)
            m_current.m_character8++;
        else
            m_current.m_character16++;
    }

    segment.command = m_previousCommand = command;

    ASSERT(m_error.status() == SVGParseStatus::NoError);

    switch (segment.command) {
    case PathSegCurveToCubicRel:
    case PathSegCurveToCubicAbs:
        segment.point1.setX(parseNumberWithError());
        segment.point1.setY(parseNumberWithError());
        /* fall through */
    case PathSegCurveToCubicSmoothRel:
    case PathSegCurveToCubicSmoothAbs:
        segment.point2.setX(parseNumberWithError());
        segment.point2.setY(parseNumberWithError());
        /* fall through */
    case PathSegMoveToRel:
    case PathSegMoveToAbs:
    case PathSegLineToRel:
    case PathSegLineToAbs:
    case PathSegCurveToQuadraticSmoothRel:
    case PathSegCurveToQuadraticSmoothAbs:
        segment.targetPoint.setX(parseNumberWithError());
        segment.targetPoint.setY(parseNumberWithError());
        break;
    case PathSegLineToHorizontalRel:
    case PathSegLineToHorizontalAbs:
        segment.targetPoint.setX(parseNumberWithError());
        break;
    case PathSegLineToVerticalRel:
    case PathSegLineToVerticalAbs:
        segment.targetPoint.setY(parseNumberWithError());
        break;
    case PathSegClosePath:
        eatWhitespace();
        break;
    case PathSegCurveToQuadraticRel:
    case PathSegCurveToQuadraticAbs:
        segment.point1.setX(parseNumberWithError());
        segment.point1.setY(parseNumberWithError());
        segment.targetPoint.setX(parseNumberWithError());
        segment.targetPoint.setY(parseNumberWithError());
        break;
    case PathSegArcRel:
    case PathSegArcAbs:
        segment.arcRadii().setX(parseNumberWithError());
        segment.arcRadii().setY(parseNumberWithError());
        segment.setArcAngle(parseNumberWithError());
        segment.arcLarge = parseArcFlagWithError();
        segment.arcSweep = parseArcFlagWithError();
        segment.targetPoint.setX(parseNumberWithError());
        segment.targetPoint.setY(parseNumberWithError());
        break;
    case PathSegUnknown:
        ASSERT_NOT_REACHED();
    }

    if (UNLIKELY(m_error.status() != SVGParseStatus::NoError))
        segment.command = PathSegUnknown;
    return segment;
}

} // namespace blink
