/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/canvas/CanvasStyle.h"

#include "core/CSSPropertyNames.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "platform/graphics/GraphicsContext.h"
#include "wtf/PassRefPtr.h"

namespace blink {

enum ColorParseResult { ParsedRGBA, ParsedCurrentColor, ParsedSystemColor, ParseFailed };

static ColorParseResult parseColor(RGBA32& parsedColor, const String& colorString)
{
    if (equalIgnoringCase(colorString, "currentcolor"))
        return ParsedCurrentColor;
    const bool useStrictParsing = true;
    if (CSSParser::parseColor(parsedColor, colorString, useStrictParsing))
        return ParsedRGBA;
    if (CSSParser::parseSystemColor(parsedColor, colorString))
        return ParsedSystemColor;
    return ParseFailed;
}

static RGBA32 currentColor(HTMLCanvasElement* canvas)
{
    if (!canvas || !canvas->inDocument() || !canvas->inlineStyle())
        return Color::black;
    RGBA32 rgba = Color::black;
    CSSParser::parseColor(rgba, canvas->inlineStyle()->getPropertyValue(CSSPropertyColor));
    return rgba;
}

bool parseColorOrCurrentColor(RGBA32& parsedColor, const String& colorString, HTMLCanvasElement* canvas)
{
    ColorParseResult parseResult = parseColor(parsedColor, colorString);
    switch (parseResult) {
    case ParsedRGBA:
    case ParsedSystemColor:
        return true;
    case ParsedCurrentColor:
        parsedColor = currentColor(canvas);
        return true;
    case ParseFailed:
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

CanvasStyle::CanvasStyle(RGBA32 rgba)
    : m_type(ColorRGBA)
    , m_rgba(rgba)
{
}

CanvasStyle::CanvasStyle(PassRefPtrWillBeRawPtr<CanvasGradient> gradient)
    : m_type(Gradient)
    , m_gradient(gradient)
{
}

CanvasStyle::CanvasStyle(PassRefPtrWillBeRawPtr<CanvasPattern> pattern)
    : m_type(ImagePattern)
    , m_pattern(pattern)
{
}

PassRefPtrWillBeRawPtr<CanvasStyle> CanvasStyle::createFromGradient(PassRefPtrWillBeRawPtr<CanvasGradient> gradient)
{
    ASSERT(gradient);
    return adoptRefWillBeNoop(new CanvasStyle(gradient));
}

PassRefPtrWillBeRawPtr<CanvasStyle> CanvasStyle::createFromPattern(PassRefPtrWillBeRawPtr<CanvasPattern> pattern)
{
    ASSERT(pattern);
    return adoptRefWillBeNoop(new CanvasStyle(pattern));
}

void CanvasStyle::applyStrokeColor(GraphicsContext* context)
{
    if (!context)
        return;
    switch (m_type) {
    case ColorRGBA:
        context->setStrokeColor(m_rgba);
        break;
    case Gradient:
        context->setStrokeGradient(canvasGradient()->gradient());
        break;
    case ImagePattern:
        context->setStrokePattern(canvasPattern()->pattern());
        break;
    }
}

void CanvasStyle::applyFillColor(GraphicsContext* context)
{
    if (!context)
        return;
    switch (m_type) {
    case ColorRGBA:
        context->setFillColor(m_rgba);
        break;
    case Gradient:
        context->setFillGradient(canvasGradient()->gradient());
        break;
    case ImagePattern:
        context->setFillPattern(canvasPattern()->pattern());
        break;
    }
}

DEFINE_TRACE(CanvasStyle)
{
    visitor->trace(m_gradient);
    visitor->trace(m_pattern);
}

}
