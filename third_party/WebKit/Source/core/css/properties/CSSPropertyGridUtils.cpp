// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyGridUtils.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

Vector<String> ParseGridTemplateAreasColumnNames(const String& grid_row_names) {
  DCHECK(!grid_row_names.IsEmpty());
  Vector<String> column_names;
  // Using StringImpl to avoid checks and indirection in every call to
  // String::operator[].
  StringImpl& text = *grid_row_names.Impl();

  StringBuilder area_name;
  for (unsigned i = 0; i < text.length(); ++i) {
    if (IsCSSSpace(text[i])) {
      if (!area_name.IsEmpty()) {
        column_names.push_back(area_name.ToString());
        area_name.Clear();
      }
      continue;
    }
    if (text[i] == '.') {
      if (area_name == ".")
        continue;
      if (!area_name.IsEmpty()) {
        column_names.push_back(area_name.ToString());
        area_name.Clear();
      }
    } else {
      if (!IsNameCodePoint(text[i]))
        return Vector<String>();
      if (area_name == ".") {
        column_names.push_back(area_name.ToString());
        area_name.Clear();
      }
    }

    area_name.Append(text[i]);
  }

  if (!area_name.IsEmpty())
    column_names.push_back(area_name.ToString());

  return column_names;
}

CSSValue* ConsumeGridBreadth(CSSParserTokenRange& range,
                             CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueMinContent,
                                             CSSValueMaxContent, CSSValueAuto>(
          token.Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  if (token.GetType() == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kFraction) {
    if (range.Peek().NumericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kFraction);
  }
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

CSSValue* ConsumeFitContent(CSSParserTokenRange& range,
                            CSSParserMode css_parser_mode) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);
  CSSPrimitiveValue* length = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      args, css_parser_mode, kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
  if (!length || !args.AtEnd())
    return nullptr;
  range = range_copy;
  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueFitContent);
  result->Append(*length);
  return result;
}

bool IsGridBreadthFixedSized(const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    return !(value_id == CSSValueMinContent || value_id == CSSValueMaxContent ||
             value_id == CSSValueAuto);
  }

  if (value.IsPrimitiveValue()) {
    return !ToCSSPrimitiveValue(value).IsFlex();
  }

  NOTREACHED();
  return true;
}

bool IsGridTrackFixedSized(const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue())
    return IsGridBreadthFixedSized(value);

  DCHECK(value.IsFunctionValue());
  auto& function = ToCSSFunctionValue(value);
  if (function.FunctionType() == CSSValueFitContent)
    return false;

  const CSSValue& min_value = function.Item(0);
  const CSSValue& max_value = function.Item(1);
  return IsGridBreadthFixedSized(min_value) ||
         IsGridBreadthFixedSized(max_value);
}

CSSValue* ConsumeGridTrackSize(CSSParserTokenRange& range,
                               CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueAuto>(token.Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (token.FunctionId() == CSSValueMinmax) {
    CSSParserTokenRange range_copy = range;
    CSSParserTokenRange args =
        CSSPropertyParserHelpers::ConsumeFunction(range_copy);
    CSSValue* min_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!min_track_breadth ||
        (min_track_breadth->IsPrimitiveValue() &&
         ToCSSPrimitiveValue(min_track_breadth)->IsFlex()) ||
        !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
      return nullptr;
    CSSValue* max_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!max_track_breadth || !args.AtEnd())
      return nullptr;
    range = range_copy;
    CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueMinmax);
    result->Append(*min_track_breadth);
    result->Append(*max_track_breadth);
    return result;
  }

  if (token.FunctionId() == CSSValueFitContent)
    return ConsumeFitContent(range, css_parser_mode);

  return ConsumeGridBreadth(range, css_parser_mode);
}

CSSCustomIdentValue* ConsumeCustomIdentForGridLine(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto || range.Peek().Id() == CSSValueSpan ||
      range.Peek().Id() == CSSValueDefault)
    return nullptr;
  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a
// new one.
CSSGridLineNamesValue* ConsumeGridLineNames(
    CSSParserTokenRange& range,
    CSSGridLineNamesValue* line_names = nullptr) {
  CSSParserTokenRange range_copy = range;
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kLeftBracketToken)
    return nullptr;
  if (!line_names)
    line_names = CSSGridLineNamesValue::Create();
  while (CSSCustomIdentValue* line_name =
             ConsumeCustomIdentForGridLine(range_copy))
    line_names->Append(*line_name);
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kRightBracketToken)
    return nullptr;
  range = range_copy;
  return line_names;
}

bool ConsumeGridTrackRepeatFunction(CSSParserTokenRange& range,
                                    CSSParserMode css_parser_mode,
                                    CSSValueList& list,
                                    bool& is_auto_repeat,
                                    bool& all_tracks_are_fixed_sized) {
  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  size_t repetitions = 1;
  is_auto_repeat =
      CSSPropertyParserHelpers::IdentMatches<CSSValueAutoFill, CSSValueAutoFit>(
          args.Peek().Id());
  CSSValueList* repeated_values;
  if (is_auto_repeat) {
    repeated_values =
        CSSGridAutoRepeatValue::Create(args.ConsumeIncludingWhitespace().Id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition =
        CSSPropertyParserHelpers::ConsumePositiveInteger(args);
    if (!repetition)
      return false;
    repetitions =
        clampTo<size_t>(repetition->GetDoubleValue(), 0, kGridMaxTracks);
    repeated_values = CSSValueList::CreateSpaceSeparated();
  }
  if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
    return false;
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(args);
  if (line_names)
    repeated_values->Append(*line_names);

  size_t number_of_tracks = 0;
  while (!args.AtEnd()) {
    CSSValue* track_size = ConsumeGridTrackSize(args, css_parser_mode);
    if (!track_size)
      return false;
    if (all_tracks_are_fixed_sized)
      all_tracks_are_fixed_sized = IsGridTrackFixedSized(*track_size);
    repeated_values->Append(*track_size);
    ++number_of_tracks;
    line_names = ConsumeGridLineNames(args);
    if (line_names)
      repeated_values->Append(*line_names);
  }
  // We should have found at least one <track-size> or else it is not a valid
  // <track-list>.
  if (!number_of_tracks)
    return false;

  if (is_auto_repeat) {
    list.Append(*repeated_values);
  } else {
    // We clamp the repetitions to a multiple of the repeat() track list's size,
    // while staying below the max grid size.
    repetitions = std::min(repetitions, kGridMaxTracks / number_of_tracks);
    for (size_t i = 0; i < repetitions; ++i) {
      for (size_t j = 0; j < repeated_values->length(); ++j)
        list.Append(repeated_values->Item(j));
    }
  }
  return true;
}

bool ConsumeGridTemplateRowsAndAreasAndColumns(bool important,
                                               CSSParserTokenRange& range,
                                               const CSSParserContext& context,
                                               CSSValue*& template_rows,
                                               CSSValue*& template_columns,
                                               CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  NamedGridAreaMap grid_area_map;
  size_t row_count = 0;
  size_t column_count = 0;
  CSSValueList* template_rows_value_list = CSSValueList::CreateSpaceSeparated();

  // Persists between loop iterations so we can use the same value for
  // consecutive <line-names> values
  CSSGridLineNamesValue* line_names = nullptr;

  do {
    // Handle leading <custom-ident>*.
    bool has_previous_line_names = line_names;
    line_names = ConsumeGridLineNames(range, line_names);
    if (line_names && !has_previous_line_names)
      template_rows_value_list->Append(*line_names);

    // Handle a template-area's row.
    if (range.Peek().GetType() != kStringToken ||
        !CSSPropertyGridUtils::ParseGridTemplateAreasRow(
            range.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count))
      return false;
    ++row_count;

    // Handle template-rows's track-size.
    CSSValue* value = ConsumeGridTrackSize(range, context.Mode());
    if (!value)
      value = CSSIdentifierValue::Create(CSSValueAuto);
    template_rows_value_list->Append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    line_names = ConsumeGridLineNames(range);
    if (line_names)
      template_rows_value_list->Append(*line_names);
  } while (!range.AtEnd() && !(range.Peek().GetType() == kDelimiterToken &&
                               range.Peek().Delimiter() == '/'));

  if (!range.AtEnd()) {
    if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    template_columns = CSSPropertyGridUtils::ConsumeGridTrackList(
        range, context.Mode(), CSSPropertyGridUtils::kGridTemplateNoRepeat);
    if (!template_columns || !range.AtEnd())
      return false;
  } else {
    template_columns = CSSIdentifierValue::Create(CSSValueNone);
  }

  template_rows = template_rows_value_list;
  template_areas =
      CSSGridTemplateAreasValue::Create(grid_area_map, row_count, column_count);
  return true;
}

}  // namespace

CSSValue* CSSPropertyGridUtils::ConsumeGridLine(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSIdentifierValue* span_value = nullptr;
  CSSCustomIdentValue* grid_line_name = nullptr;
  CSSPrimitiveValue* numeric_value =
      CSSPropertyParserHelpers::ConsumeInteger(range);
  if (numeric_value) {
    grid_line_name = ConsumeCustomIdentForGridLine(range);
    span_value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
  } else {
    span_value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
    if (span_value) {
      numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
      grid_line_name = ConsumeCustomIdentForGridLine(range);
      if (!numeric_value)
        numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
    } else {
      grid_line_name = ConsumeCustomIdentForGridLine(range);
      if (grid_line_name) {
        numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
        span_value =
            CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
        if (!span_value && !numeric_value)
          return grid_line_name;
      } else {
        return nullptr;
      }
    }
  }

  if (span_value && !numeric_value && !grid_line_name)
    return nullptr;  // "span" keyword alone is invalid.
  if (span_value && numeric_value && numeric_value->GetIntValue() < 0)
    return nullptr;  // Negative numbers are not allowed for span.
  if (numeric_value && numeric_value->GetIntValue() == 0)
    return nullptr;  // An <integer> value of zero makes the declaration
                     // invalid.

  if (numeric_value) {
    numeric_value = CSSPrimitiveValue::Create(
        clampTo(numeric_value->GetIntValue(), -kGridMaxTracks, kGridMaxTracks),
        CSSPrimitiveValue::UnitType::kInteger);
  }

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (span_value)
    values->Append(*span_value);
  if (numeric_value)
    values->Append(*numeric_value);
  if (grid_line_name)
    values->Append(*grid_line_name);
  DCHECK(values->length());
  return values;
}

CSSValue* CSSPropertyGridUtils::ConsumeGridTrackList(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    TrackListType track_list_type) {
  bool allow_grid_line_names = track_list_type != kGridAuto;
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(range);
  if (line_names) {
    if (!allow_grid_line_names)
      return nullptr;
    values->Append(*line_names);
  }

  bool allow_repeat = track_list_type == kGridTemplate;
  bool seen_auto_repeat = false;
  bool all_tracks_are_fixed_sized = true;
  do {
    bool is_auto_repeat;
    if (range.Peek().FunctionId() == CSSValueRepeat) {
      if (!allow_repeat)
        return nullptr;
      if (!ConsumeGridTrackRepeatFunction(range, css_parser_mode, *values,
                                          is_auto_repeat,
                                          all_tracks_are_fixed_sized))
        return nullptr;
      if (is_auto_repeat && seen_auto_repeat)
        return nullptr;
      seen_auto_repeat = seen_auto_repeat || is_auto_repeat;
    } else if (CSSValue* value = ConsumeGridTrackSize(range, css_parser_mode)) {
      if (all_tracks_are_fixed_sized)
        all_tracks_are_fixed_sized = IsGridTrackFixedSized(*value);
      values->Append(*value);
    } else {
      return nullptr;
    }
    if (seen_auto_repeat && !all_tracks_are_fixed_sized)
      return nullptr;
    line_names = ConsumeGridLineNames(range);
    if (line_names) {
      if (!allow_grid_line_names)
        return nullptr;
      values->Append(*line_names);
    }
  } while (!range.AtEnd() && range.Peek().GetType() != kDelimiterToken);
  return values;
}

bool CSSPropertyGridUtils::ParseGridTemplateAreasRow(
    const String& grid_row_names,
    NamedGridAreaMap& grid_area_map,
    const size_t row_count,
    size_t& column_count) {
  if (grid_row_names.IsEmpty() || grid_row_names.ContainsOnlyWhitespace())
    return false;

  Vector<String> column_names =
      ParseGridTemplateAreasColumnNames(grid_row_names);
  if (row_count == 0) {
    column_count = column_names.size();
    if (column_count == 0)
      return false;
  } else if (column_count != column_names.size()) {
    // The declaration is invalid if all the rows don't have the number of
    // columns.
    return false;
  }

  for (size_t current_column = 0; current_column < column_count;
       ++current_column) {
    const String& grid_area_name = column_names[current_column];

    // Unamed areas are always valid (we consider them to be 1x1).
    if (grid_area_name == ".")
      continue;

    size_t look_ahead_column = current_column + 1;
    while (look_ahead_column < column_count &&
           column_names[look_ahead_column] == grid_area_name)
      look_ahead_column++;

    NamedGridAreaMap::iterator grid_area_it =
        grid_area_map.find(grid_area_name);
    if (grid_area_it == grid_area_map.end()) {
      grid_area_map.insert(grid_area_name,
                           GridArea(GridSpan::TranslatedDefiniteGridSpan(
                                        row_count, row_count + 1),
                                    GridSpan::TranslatedDefiniteGridSpan(
                                        current_column, look_ahead_column)));
    } else {
      GridArea& grid_area = grid_area_it->value;

      // The following checks test that the grid area is a single filled-in
      // rectangle.
      // 1. The new row is adjacent to the previously parsed row.
      if (row_count != grid_area.rows.EndLine())
        return false;

      // 2. The new area starts at the same position as the previously parsed
      // area.
      if (current_column != grid_area.columns.StartLine())
        return false;

      // 3. The new area ends at the same position as the previously parsed
      // area.
      if (look_ahead_column != grid_area.columns.EndLine())
        return false;

      grid_area.rows = GridSpan::TranslatedDefiniteGridSpan(
          grid_area.rows.StartLine(), grid_area.rows.EndLine() + 1);
    }
    current_column = look_ahead_column - 1;
  }

  return true;
}

CSSValue* CSSPropertyGridUtils::ConsumeGridTemplatesRowsOrColumns(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return ConsumeGridTrackList(range, css_parser_mode, kGridTemplate);
}

bool CSSPropertyGridUtils::ConsumeGridItemPositionShorthand(
    bool important,
    CSSParserTokenRange& range,
    CSSValue*& start_value,
    CSSValue*& end_value) {
  // Input should be nullptrs.
  DCHECK(!start_value);
  DCHECK(!end_value);
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());

  start_value = ConsumeGridLine(range);
  if (!start_value)
    return false;

  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    end_value = ConsumeGridLine(range);
    if (!end_value)
      return false;
  } else {
    end_value = start_value->IsCustomIdentValue()
                    ? start_value
                    : CSSIdentifierValue::Create(CSSValueAuto);
  }

  return range.AtEnd();
}

bool CSSPropertyGridUtils::ConsumeGridTemplateShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSValue*& template_rows,
    CSSValue*& template_columns,
    CSSValue*& template_areas) {
  DCHECK(!template_rows);
  DCHECK(!template_columns);
  DCHECK(!template_areas);

  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  DCHECK_EQ(gridTemplateShorthand().length(), 3u);

  CSSParserTokenRange range_copy = range;
  template_rows = CSSPropertyParserHelpers::ConsumeIdent<CSSValueNone>(range);

  // 1- 'none' case.
  if (template_rows && range.AtEnd()) {
    template_rows = CSSIdentifierValue::Create(CSSValueNone);
    template_columns = CSSIdentifierValue::Create(CSSValueNone);
    template_areas = CSSIdentifierValue::Create(CSSValueNone);
    return true;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!template_rows)
    template_rows = ConsumeGridTrackList(range, context.Mode(), kGridTemplate);

  if (template_rows) {
    if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
      return false;
    template_columns = ConsumeGridTemplatesRowsOrColumns(range, context.Mode());
    if (!template_columns || !range.AtEnd())
      return false;

    template_areas = CSSIdentifierValue::Create(CSSValueNone);
    return true;
  }

  // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+
  // [ / <track-list> ]?
  range = range_copy;
  return ConsumeGridTemplateRowsAndAreasAndColumns(
      important, range, context, template_rows, template_columns,
      template_areas);
}

}  // namespace blink
