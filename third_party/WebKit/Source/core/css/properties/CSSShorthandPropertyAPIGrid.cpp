// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIGrid.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyGridUtils.h"

namespace blink {

namespace {

CSSValueList* ConsumeImplicitAutoFlow(CSSParserTokenRange& range,
                                      const CSSValue& flow_direction) {
  // [ auto-flow && dense? ]
  CSSValue* dense_algorithm = nullptr;
  if ((CSSPropertyParserHelpers::ConsumeIdent<CSSValueAutoFlow>(range))) {
    dense_algorithm =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueDense>(range);
  } else {
    dense_algorithm =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueDense>(range);
    if (!dense_algorithm)
      return nullptr;
    if (!CSSPropertyParserHelpers::ConsumeIdent<CSSValueAutoFlow>(range))
      return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(flow_direction);
  if (dense_algorithm)
    list->Append(*dense_algorithm);
  return list;
}
}  // namespace

bool CSSShorthandPropertyAPIGrid::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool,
    HeapVector<CSSProperty, 256>& properties) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  DCHECK_EQ(shorthandForProperty(CSSPropertyGrid).length(), 8u);

  CSSParserTokenRange range_copy = range;

  CSSValue* template_rows = nullptr;
  CSSValue* template_columns = nullptr;
  CSSValue* template_areas = nullptr;

  if (CSSPropertyGridUtils::ConsumeGridTemplateShorthand(
          important, range, context, template_rows, template_columns,
          template_areas)) {
    DCHECK(template_rows);
    DCHECK(template_columns);
    DCHECK(template_areas);

    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridTemplateRows, CSSPropertyGrid, *template_rows, important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridTemplateColumns, CSSPropertyGrid, *template_columns,
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridTemplateAreas, CSSPropertyGrid, *template_areas,
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);

    // It can only be specified the explicit or the implicit grid properties in
    // a single grid declaration. The sub-properties not specified are set to
    // their initial value, as normal for shorthands.
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridAutoFlow, CSSPropertyGrid, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridAutoColumns, CSSPropertyGrid, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridAutoRows, CSSPropertyGrid, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridColumnGap, CSSPropertyGrid, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyGridRowGap, CSSPropertyGrid, *CSSInitialValue::Create(),
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
    return true;
  }

  range = range_copy;

  CSSValue* auto_columns_value = nullptr;
  CSSValue* auto_rows_value = nullptr;
  CSSValueList* grid_auto_flow = nullptr;
  template_rows = nullptr;
  template_columns = nullptr;

  if (CSSPropertyParserHelpers::IdentMatches<CSSValueDense, CSSValueAutoFlow>(
          range.Peek().Id())) {
    // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range, *CSSIdentifierValue::Create(CSSValueRow));
    if (!grid_auto_flow)
      return false;
    if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
      auto_rows_value = CSSInitialValue::Create();
    } else {
      auto_rows_value = CSSPropertyGridUtils::ConsumeGridTrackList(
          range, context.Mode(), CSSPropertyGridUtils::kGridAuto);
      if (!auto_rows_value)
        return false;
      if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
        return false;
    }
    if (!(template_columns =
              CSSPropertyGridUtils::ConsumeGridTemplatesRowsOrColumns(
                  range, context.Mode())))
      return false;
    template_rows = CSSInitialValue::Create();
    auto_columns_value = CSSInitialValue::Create();
  } else {
    // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
    template_rows = CSSPropertyGridUtils::ConsumeGridTemplatesRowsOrColumns(
        range, context.Mode());
    if (!template_rows)
      return false;
    if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range, *CSSIdentifierValue::Create(CSSValueColumn));
    if (!grid_auto_flow)
      return false;
    if (range.AtEnd()) {
      auto_columns_value = CSSInitialValue::Create();
    } else {
      auto_columns_value = CSSPropertyGridUtils::ConsumeGridTrackList(
          range, context.Mode(), CSSPropertyGridUtils::kGridAuto);
      if (!auto_columns_value)
        return false;
    }
    template_columns = CSSInitialValue::Create();
    auto_rows_value = CSSInitialValue::Create();
  }

  if (!range.AtEnd())
    return false;

  // It can only be specified the explicit or the implicit grid properties in a
  // single grid declaration. The sub-properties not specified are set to their
  // initial value, as normal for shorthands.
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridTemplateColumns, CSSPropertyGrid, *template_columns,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridTemplateRows, CSSPropertyGrid, *template_rows, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridTemplateAreas, CSSPropertyGrid, *CSSInitialValue::Create(),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridAutoFlow, CSSPropertyGrid, *grid_auto_flow, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridAutoColumns, CSSPropertyGrid, *auto_columns_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridAutoRows, CSSPropertyGrid, *auto_rows_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridColumnGap, CSSPropertyGrid, *CSSInitialValue::Create(),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridRowGap, CSSPropertyGrid, *CSSInitialValue::Create(),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

}  // namespace blink
