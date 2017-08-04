// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyGridUtils_h
#define CSSPropertyGridUtils_h

#include "core/css/parser/CSSParserMode.h"
#include "core/style/GridArea.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyGridUtils {
  STATIC_ONLY(CSSPropertyGridUtils);

 public:
  // TODO(jiameng): move the following functions to anonymous namespace after
  // all grid-related shorthands have their APIs implemented:
  // - TrackListType
  // - ConsumeGridLine
  enum TrackListType { kGridTemplate, kGridTemplateNoRepeat, kGridAuto };
  static CSSValue* ConsumeGridLine(CSSParserTokenRange&);
  static CSSValue* ConsumeGridTrackList(CSSParserTokenRange&,
                                        CSSParserMode,
                                        TrackListType);
  static bool ParseGridTemplateAreasRow(const WTF::String& grid_row_names,
                                        NamedGridAreaMap&,
                                        const size_t row_count,
                                        size_t& column_count);
  static CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenRange&,
                                                     CSSParserMode);
  static bool ConsumeGridItemPositionShorthand(bool important,
                                               CSSParserTokenRange&,
                                               CSSValue*& start_value,
                                               CSSValue*& end_value);
  static bool ConsumeGridTemplateShorthand(bool important,
                                           CSSParserTokenRange&,
                                           const CSSParserContext&,
                                           CSSValue*& template_rows,
                                           CSSValue*& template_columns,
                                           CSSValue*& template_areas);
};

}  // namespace blink

#endif  // CSSPropertyGridUtils_h
