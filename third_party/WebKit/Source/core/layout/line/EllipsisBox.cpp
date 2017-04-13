/**
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/line/EllipsisBox.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/TextRunConstructor.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/EllipsisBoxPainter.h"
#include "core/style/ShadowList.h"
#include "platform/fonts/Font.h"
#include "platform/text/TextRun.h"

namespace blink {

void EllipsisBox::Paint(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset,
                        LayoutUnit line_top,
                        LayoutUnit line_bottom) const {
  EllipsisBoxPainter(*this).Paint(paint_info, paint_offset, line_top,
                                  line_bottom);
}

IntRect EllipsisBox::SelectionRect() const {
  const ComputedStyle& style = GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  const Font& font = style.GetFont();
  return EnclosingIntRect(font.SelectionRectForText(
      ConstructTextRun(font, str_, style, TextRun::kAllowTrailingExpansion),
      IntPoint(LogicalLeft().ToInt(),
               (LogicalTop() + Root().SelectionTop()).ToInt()),
      Root().SelectionHeight().ToInt()));
}

bool EllipsisBox::NodeAtPoint(HitTestResult& result,
                              const HitTestLocation& location_in_container,
                              const LayoutPoint& accumulated_offset,
                              LayoutUnit line_top,
                              LayoutUnit line_bottom) {
  LayoutPoint adjusted_location = accumulated_offset + Location();

  LayoutPoint box_origin = PhysicalLocation();
  box_origin.MoveBy(accumulated_offset);
  LayoutRect bounds_rect(box_origin, Size());
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      bounds_rect.Intersects(LayoutRect(HitTestLocation::RectForPoint(
          location_in_container.Point(), 0, 0, 0, 0)))) {
    GetLineLayoutItem().UpdateHitTestResult(
        result,
        location_in_container.Point() - ToLayoutSize(adjusted_location));
    if (result.AddNodeToListBasedTestResult(GetLineLayoutItem().GetNode(),
                                            location_in_container,
                                            bounds_rect) == kStopHitTesting)
      return true;
  }

  return false;
}

const char* EllipsisBox::BoxName() const {
  return "EllipsisBox";
}

}  // namespace blink
