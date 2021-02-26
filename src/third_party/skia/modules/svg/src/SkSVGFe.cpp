/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/effects/SkImageFilters.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGFe.h"

sk_sp<SkImageFilter> SkSVGFe::makeImageFilter(const SkSVGRenderContext& ctx,
                                              const SkSVGFilterContext& fctx) const {
    return this->onMakeImageFilter(ctx, fctx);
}

bool SkSVGFe::parseAndSetAttribute(const char* name, const char* value) {
    return INHERITED::parseAndSetAttribute(name, value) ||
           this->setIn(SkSVGAttributeParser::parse<SkSVGFeInputType>("in", name, value)) ||
           this->setResult(SkSVGAttributeParser::parse<SkSVGStringType>("result", name, value));
}

template <> bool SkSVGAttributeParser::parse(SkSVGFeInputType* type) {
    static constexpr std::tuple<const char*, SkSVGFeInputType::Type> gTypeMap[] = {
            {"SourceGraphic", SkSVGFeInputType::Type::kSourceGraphic},
            {"SourceAlpha", SkSVGFeInputType::Type::kSourceAlpha},
            {"BackgroundImage", SkSVGFeInputType::Type::kBackgroundImage},
            {"BackgroundAlpha", SkSVGFeInputType::Type::kBackgroundAlpha},
            {"FillPaint", SkSVGFeInputType::Type::kFillPaint},
            {"StrokePaint", SkSVGFeInputType::Type::kStrokePaint},
    };

    SkSVGStringType resultId;
    SkSVGFeInputType::Type t;
    bool parsedValue = false;
    if (this->parseEnumMap(gTypeMap, &t)) {
        *type = SkSVGFeInputType(t);
        parsedValue = true;
    } else if (parse(&resultId)) {
        *type = SkSVGFeInputType(resultId);
        parsedValue = true;
    }

    return parsedValue && this->parseEOSToken();
}
