// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebSelection.h"

#include "core/editing/SelectionType.h"
#include "core/layout/compositing/CompositedSelection.h"

namespace blink {

static WebSelectionBound GetWebSelectionBound(
    const CompositedSelection& selection,
    bool is_start) {
  DCHECK_NE(selection.type, kNoSelection);
  const CompositedSelectionBound& bound =
      is_start ? selection.start : selection.end;
  DCHECK(bound.layer);

  WebSelectionBound::Type type = WebSelectionBound::kCaret;
  if (selection.type == kRangeSelection) {
    if (is_start) {
      type = bound.is_text_direction_rtl ? WebSelectionBound::kSelectionRight
                                         : WebSelectionBound::kSelectionLeft;
    } else {
      type = bound.is_text_direction_rtl ? WebSelectionBound::kSelectionLeft
                                         : WebSelectionBound::kSelectionRight;
    }
  }

  WebSelectionBound result(type);
  result.layer_id = bound.layer->PlatformLayer()->Id();
  result.edge_top_in_layer = RoundedIntPoint(bound.edge_top_in_layer);
  result.edge_bottom_in_layer = RoundedIntPoint(bound.edge_bottom_in_layer);
  result.is_text_direction_rtl = bound.is_text_direction_rtl;
  return result;
}

// SelectionType enums have the same values; enforced in
// AssertMatchingEnums.cpp.
WebSelection::WebSelection(const CompositedSelection& selection)
    : selection_type_(static_cast<WebSelection::SelectionType>(selection.type)),
      start_(GetWebSelectionBound(selection, true)),
      end_(GetWebSelectionBound(selection, false)) {}

WebSelection::WebSelection(const WebSelection& other)
    : selection_type_(other.selection_type_),
      start_(other.start_),
      end_(other.end_) {}

}  // namespace blink
