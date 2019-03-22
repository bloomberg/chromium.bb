// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/grid.h"

#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace {

CSSValueList* ConsumeImplicitAutoFlow(CSSParserTokenRange& range,
                                      const CSSValue& flow_direction) {
  // [ auto-flow && dense? ]
  CSSValue* dense_algorithm = nullptr;
  if ((css_property_parser_helpers::ConsumeIdent<CSSValueAutoFlow>(range))) {
    dense_algorithm =
        css_property_parser_helpers::ConsumeIdent<CSSValueDense>(range);
  } else {
    dense_algorithm =
        css_property_parser_helpers::ConsumeIdent<CSSValueDense>(range);
    if (!dense_algorithm)
      return nullptr;
    if (!css_property_parser_helpers::ConsumeIdent<CSSValueAutoFlow>(range))
      return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(flow_direction);
  if (dense_algorithm)
    list->Append(*dense_algorithm);
  return list;
}

}  // namespace
namespace css_shorthand {

bool Grid::ParseShorthand(bool important,
                          CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          const CSSParserLocalContext&,
                          HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyGrid).length(), 6u);

  CSSParserTokenRange range_copy = range;

  CSSValue* template_rows = nullptr;
  CSSValue* template_columns = nullptr;
  CSSValue* template_areas = nullptr;

  if (css_parsing_utils::ConsumeGridTemplateShorthand(
          important, range, context, template_rows, template_columns,
          template_areas)) {
    DCHECK(template_rows);
    DCHECK(template_columns);
    DCHECK(template_areas);

    css_property_parser_helpers::AddProperty(
        CSSPropertyGridTemplateRows, CSSPropertyGrid, *template_rows, important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
    css_property_parser_helpers::AddProperty(
        CSSPropertyGridTemplateColumns, CSSPropertyGrid, *template_columns,
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
    css_property_parser_helpers::AddProperty(
        CSSPropertyGridTemplateAreas, CSSPropertyGrid, *template_areas,
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);

    // It can only be specified the explicit or the implicit grid properties in
    // a single grid declaration. The sub-properties not specified are set to
    // their initial value, as normal for shorthands.
    css_property_parser_helpers::AddProperty(
        CSSPropertyGridAutoFlow, CSSPropertyGrid, *CSSInitialValue::Create(),
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
    css_property_parser_helpers::AddProperty(
        CSSPropertyGridAutoColumns, CSSPropertyGrid, *CSSInitialValue::Create(),
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
    css_property_parser_helpers::AddProperty(
        CSSPropertyGridAutoRows, CSSPropertyGrid, *CSSInitialValue::Create(),
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
    return true;
  }

  range = range_copy;

  CSSValue* auto_columns_value = nullptr;
  CSSValue* auto_rows_value = nullptr;
  CSSValueList* grid_auto_flow = nullptr;
  template_rows = nullptr;
  template_columns = nullptr;

  if (css_property_parser_helpers::IdentMatches<CSSValueDense,
                                                CSSValueAutoFlow>(
          range.Peek().Id())) {
    // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range, *CSSIdentifierValue::Create(CSSValueRow));
    if (!grid_auto_flow)
      return false;
    if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range)) {
      auto_rows_value = CSSInitialValue::Create();
    } else {
      auto_rows_value = css_parsing_utils::ConsumeGridTrackList(
          range, context, context.Mode(),
          css_parsing_utils::TrackListType::kGridAuto);
      if (!auto_rows_value)
        return false;
      if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range))
        return false;
    }
    if (!(template_columns =
              css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(
                  range, context, context.Mode())))
      return false;
    template_rows = CSSInitialValue::Create();
    auto_columns_value = CSSInitialValue::Create();
  } else {
    // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
    template_rows = css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(
        range, context, context.Mode());
    if (!template_rows)
      return false;
    if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range, *CSSIdentifierValue::Create(CSSValueColumn));
    if (!grid_auto_flow)
      return false;
    if (range.AtEnd()) {
      auto_columns_value = CSSInitialValue::Create();
    } else {
      auto_columns_value = css_parsing_utils::ConsumeGridTrackList(
          range, context, context.Mode(),
          css_parsing_utils::TrackListType::kGridAuto);
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
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridTemplateColumns, CSSPropertyGrid, *template_columns,
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridTemplateRows, CSSPropertyGrid, *template_rows, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridTemplateAreas, CSSPropertyGrid, *CSSInitialValue::Create(),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridAutoFlow, CSSPropertyGrid, *grid_auto_flow, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridAutoColumns, CSSPropertyGrid, *auto_columns_value,
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridAutoRows, CSSPropertyGrid, *auto_rows_value, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

bool Grid::IsLayoutDependent(const ComputedStyle* style,
                             LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* Grid::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridShorthand(
      gridShorthand(), style, layout_object, styled_node, allow_visited_style);
}

}  // namespace css_shorthand
}  // namespace blink
