/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef GraphicsTypes_h
#define GraphicsTypes_h

#include "third_party/skia/include/core/SkPaint.h"

#include "wtf/Forward.h"

namespace WebCore {

enum StrokeStyle {
    NoStroke,
    SolidStroke,
    DottedStroke,
    DashedStroke,
    DoubleStroke,
    WavyStroke,
};

enum InterpolationQuality {
    InterpolationDefault,
    InterpolationNone,
    InterpolationLow,
    InterpolationMedium,
    InterpolationHigh
};

enum CompositeOperator {
    CompositeClear,
    CompositeCopy,
    CompositeSourceOver,
    CompositeSourceIn,
    CompositeSourceOut,
    CompositeSourceAtop,
    CompositeDestinationOver,
    CompositeDestinationIn,
    CompositeDestinationOut,
    CompositeDestinationAtop,
    CompositeXOR,
    CompositePlusDarker,
    CompositePlusLighter,
    CompositeDifference
};

// keep it in sync with gMapBlendOpsToXfermodeModes array in SkiaUtils.h
enum BlendMode {
    BlendModeNormal,
    BlendModeMultiply,
    BlendModeScreen,
    BlendModeOverlay,
    BlendModeDarken,
    BlendModeLighten,
    BlendModeColorDodge,
    BlendModeColorBurn,
    BlendModeHardLight,
    BlendModeSoftLight,
    BlendModeDifference,
    BlendModeExclusion,
    BlendModeHue,
    BlendModeSaturation,
    BlendModeColor,
    BlendModeLuminosity
};

enum GradientSpreadMethod {
    SpreadMethodPad,
    SpreadMethodReflect,
    SpreadMethodRepeat
};

enum LineCap {
    ButtCap = SkPaint::kButt_Cap,
    RoundCap = SkPaint::kRound_Cap,
    SquareCap = SkPaint::kSquare_Cap
};

enum LineJoin {
    MiterJoin = SkPaint::kMiter_Join,
    RoundJoin = SkPaint::kRound_Join,
    BevelJoin = SkPaint::kBevel_Join
};

enum HorizontalAlignment { AlignLeft, AlignRight, AlignHCenter };

enum TextBaseline { AlphabeticTextBaseline, TopTextBaseline, MiddleTextBaseline, BottomTextBaseline, IdeographicTextBaseline, HangingTextBaseline };

enum TextAlign { StartTextAlign, EndTextAlign, LeftTextAlign, CenterTextAlign, RightTextAlign };

enum TextDrawingMode {
    TextModeFill      = 1 << 0,
    TextModeStroke    = 1 << 1,
};
typedef unsigned TextDrawingModeFlags;

String compositeOperatorName(CompositeOperator, BlendMode);
bool parseCompositeAndBlendOperator(const String&, CompositeOperator&, BlendMode&);

String lineCapName(LineCap);
bool parseLineCap(const String&, LineCap&);

String lineJoinName(LineJoin);
bool parseLineJoin(const String&, LineJoin&);

String textAlignName(TextAlign);
bool parseTextAlign(const String&, TextAlign&);

String textBaselineName(TextBaseline);
bool parseTextBaseline(const String&, TextBaseline&);

} // namespace WebCore

#endif
