/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/private/SkTPin.h"
#include "modules/svg/include/SkSVGGradient.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGStop.h"
#include "modules/svg/include/SkSVGValue.h"

bool SkSVGGradient::parseAndSetAttribute(const char* name, const char* value) {
    return INHERITED::parseAndSetAttribute(name, value) ||
           this->setGradientTransform(SkSVGAttributeParser::parse<SkSVGTransformType>(
                   "gradientTransform", name, value)) ||
           this->setHref(SkSVGAttributeParser::parse<SkSVGIRI>("xlink:href", name, value)) ||
           this->setSpreadMethod(
                   SkSVGAttributeParser::parse<SkSVGSpreadMethod>("spreadMethod", name, value)) ||
           this->setGradientUnits(SkSVGAttributeParser::parse<SkSVGObjectBoundingBoxUnits>(
                   "gradientUnits", name, value));
}

// https://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementHrefAttribute
void SkSVGGradient::collectColorStops(const SkSVGRenderContext& ctx,
                                      StopPositionArray* pos,
                                      StopColorArray* colors) const {
    // Used to resolve percentage offsets.
    const SkSVGLengthContext ltx(SkSize::Make(1, 1));

    for (const auto& child : fChildren) {
        if (child->tag() != SkSVGTag::kStop) {
            continue;
        }

        const auto& stop = static_cast<const SkSVGStop&>(*child);
        colors->push_back(this->resolveStopColor(ctx, stop));
        pos->push_back(SkTPin(ltx.resolve(stop.offset(), SkSVGLengthContext::LengthType::kOther),
                              0.f, 1.f));
    }

    SkASSERT(colors->count() == pos->count());

    if (pos->empty() && !fHref.fIRI.isEmpty()) {
        const auto ref = ctx.findNodeById(fHref.fIRI);
        if (ref && (ref->tag() == SkSVGTag::kLinearGradient ||
                    ref->tag() == SkSVGTag::kRadialGradient)) {
            static_cast<const SkSVGGradient*>(ref.get())->collectColorStops(ctx, pos, colors);
        }
    }
}

SkColor SkSVGGradient::resolveStopColor(const SkSVGRenderContext& ctx,
                                        const SkSVGStop& stop) const {
    const SkSVGStopColor& stopColor = stop.stopColor();
    SkColor color;
    switch (stopColor.type()) {
        case SkSVGStopColor::Type::kColor:
            color = stopColor.color();
            break;
        case SkSVGStopColor::Type::kCurrentColor:
            color = *ctx.presentationContext().fInherited.fColor;
            break;
        case SkSVGStopColor::Type::kICCColor:
            SkDebugf("unimplemented 'icccolor' stop-color type\n");
            color = SK_ColorBLACK;
            break;
        case SkSVGStopColor::Type::kInherit:
            SkDebugf("unimplemented 'inherit' stop-color type\n");
            color = SK_ColorBLACK;
            break;
    }
    return SkColorSetA(color, SkScalarRoundToInt(stop.stopOpacity() * 255));
}

bool SkSVGGradient::onAsPaint(const SkSVGRenderContext& ctx, SkPaint* paint) const {
    StopColorArray colors;
    StopPositionArray pos;

    this->collectColorStops(ctx, &pos, &colors);

    // TODO:
    //       * stop (lazy?) sorting
    //       * href loop detection
    //       * href attribute inheritance (not just color stops)
    //       * objectBoundingBox units support

    static_assert(static_cast<SkTileMode>(SkSVGSpreadMethod::Type::kPad) ==
                  SkTileMode::kClamp, "SkSVGSpreadMethod::Type is out of sync");
    static_assert(static_cast<SkTileMode>(SkSVGSpreadMethod::Type::kRepeat) ==
                  SkTileMode::kRepeat, "SkSVGSpreadMethod::Type is out of sync");
    static_assert(static_cast<SkTileMode>(SkSVGSpreadMethod::Type::kReflect) ==
                  SkTileMode::kMirror, "SkSVGSpreadMethod::Type is out of sync");
    const auto tileMode = static_cast<SkTileMode>(fSpreadMethod.type());

    SkMatrix localMatrix = SkMatrix::I();
    if (fGradientUnits.type() == SkSVGObjectBoundingBoxUnits::Type::kObjectBoundingBox) {
        SkASSERT(ctx.node());
        const SkRect objBounds = ctx.node()->objectBoundingBox(ctx);
        localMatrix.preTranslate(objBounds.fLeft, objBounds.fTop);
        localMatrix.preScale(objBounds.width(), objBounds.height());
    }
    localMatrix.preConcat(fGradientTransform);

    paint->setShader(this->onMakeShader(ctx, colors.begin(), pos.begin(), colors.count(), tileMode,
                                        localMatrix));
    return true;
}

// https://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementSpreadMethodAttribute
template <>
bool SkSVGAttributeParser::parse(SkSVGSpreadMethod* spread) {
    static const struct {
        SkSVGSpreadMethod::Type fType;
        const char*             fName;
    } gSpreadInfo[] = {
        { SkSVGSpreadMethod::Type::kPad    , "pad"     },
        { SkSVGSpreadMethod::Type::kReflect, "reflect" },
        { SkSVGSpreadMethod::Type::kRepeat , "repeat"  },
    };

    bool parsedValue = false;
    for (size_t i = 0; i < SK_ARRAY_COUNT(gSpreadInfo); ++i) {
        if (this->parseExpectedStringToken(gSpreadInfo[i].fName)) {
            *spread = SkSVGSpreadMethod(gSpreadInfo[i].fType);
            parsedValue = true;
            break;
        }
    }

    return parsedValue && this->parseEOSToken();
}
