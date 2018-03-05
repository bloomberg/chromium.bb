// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/GridTemplate.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool GridTemplate::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValue* template_rows = nullptr;
  CSSValue* template_columns = nullptr;
  CSSValue* template_areas = nullptr;
  if (!CSSParsingUtils::ConsumeGridTemplateShorthand(
          important, range, context, template_rows, template_columns,
          template_areas))
    return false;

  DCHECK(template_rows);
  DCHECK(template_columns);
  DCHECK(template_areas);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridTemplateRows, CSSPropertyGridTemplate, *template_rows,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridTemplateColumns, CSSPropertyGridTemplate,
      *template_columns, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridTemplateAreas, CSSPropertyGridTemplate, *template_areas,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

bool GridTemplate::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridShorthand(
      gridTemplateShorthand(), style, layout_object, styled_node,
      allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
