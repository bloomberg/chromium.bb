/*
 * Copyright (C) 2006, 2007, 2012 Apple Inc. All rights reserved.
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
 *
 */

#include "core/layout/LayoutFileUploadControl.h"

#include <math.h>
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ShadowRoot.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/fileapi/FileList.h"
#include "core/html/HTMLInputElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/paint/FileUploadControlPainter.h"
#include "platform/fonts/Font.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/TextRun.h"

namespace blink {

using namespace HTMLNames;

const int kDefaultWidthNumChars = 34;

LayoutFileUploadControl::LayoutFileUploadControl(HTMLInputElement* input)
    : LayoutBlockFlow(input),
      can_receive_dropped_files_(input->CanReceiveDroppedFiles()) {}

LayoutFileUploadControl::~LayoutFileUploadControl() {}

void LayoutFileUploadControl::UpdateFromElement() {
  HTMLInputElement* input = toHTMLInputElement(GetNode());
  DCHECK_EQ(input->type(), InputTypeNames::file);

  if (HTMLInputElement* button = UploadButton()) {
    bool new_can_receive_dropped_files_state = input->CanReceiveDroppedFiles();
    if (can_receive_dropped_files_ != new_can_receive_dropped_files_state) {
      can_receive_dropped_files_ = new_can_receive_dropped_files_state;
      button->SetActive(new_can_receive_dropped_files_state);
    }
  }

  // This only supports clearing out the files, but that's OK because for
  // security reasons that's the only change the DOM is allowed to make.
  FileList* files = input->files();
  DCHECK(files);
  if (files && files->IsEmpty())
    SetShouldDoFullPaintInvalidation();
}

int LayoutFileUploadControl::MaxFilenameWidth() const {
  int upload_button_width =
      (UploadButton() && UploadButton()->GetLayoutBox())
          ? UploadButton()->GetLayoutBox()->PixelSnappedWidth()
          : 0;
  return std::max(0, ContentBoxRect().PixelSnappedWidth() -
                         upload_button_width - kAfterButtonSpacing);
}

void LayoutFileUploadControl::PaintObject(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  FileUploadControlPainter(*this).PaintObject(paint_info, paint_offset);
}

void LayoutFileUploadControl::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  // Figure out how big the filename space needs to be for a given number of
  // characters (using "0" as the nominal character).
  const UChar kCharacter = '0';
  const String character_as_string = String(&kCharacter, 1);
  const Font& font = Style()->GetFont();
  float min_default_label_width =
      kDefaultWidthNumChars *
      font.Width(ConstructTextRun(font, character_as_string, StyleRef(),
                                  TextRun::kAllowTrailingExpansion));

  const String label = toHTMLInputElement(GetNode())->GetLocale().QueryString(
      WebLocalizedString::kFileButtonNoFileSelectedLabel);
  float default_label_width = font.Width(ConstructTextRun(
      font, label, StyleRef(), TextRun::kAllowTrailingExpansion));
  if (HTMLInputElement* button = UploadButton()) {
    if (LayoutObject* button_layout_object = button->GetLayoutObject())
      default_label_width += button_layout_object->MaxPreferredLogicalWidth() +
                             kAfterButtonSpacing;
  }
  max_logical_width =
      LayoutUnit(ceilf(std::max(min_default_label_width, default_label_width)));

  if (!Style()->Width().IsPercentOrCalc())
    min_logical_width = max_logical_width;
}

void LayoutFileUploadControl::ComputePreferredLogicalWidths() {
  DCHECK(PreferredLogicalWidthsDirty());

  min_preferred_logical_width_ = LayoutUnit();
  max_preferred_logical_width_ = LayoutUnit();
  const ComputedStyle& style_to_use = StyleRef();

  if (style_to_use.Width().IsFixed() && style_to_use.Width().Value() > 0)
    min_preferred_logical_width_ = max_preferred_logical_width_ =
        AdjustContentBoxLogicalWidthForBoxSizing(
            LayoutUnit(style_to_use.Width().Value()));
  else
    ComputeIntrinsicLogicalWidths(min_preferred_logical_width_,
                                  max_preferred_logical_width_);

  if (style_to_use.MinWidth().IsFixed() &&
      style_to_use.MinWidth().Value() > 0) {
    max_preferred_logical_width_ =
        std::max(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MinWidth().Value())));
    min_preferred_logical_width_ =
        std::max(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MinWidth().Value())));
  }

  if (style_to_use.MaxWidth().IsFixed()) {
    max_preferred_logical_width_ =
        std::min(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MaxWidth().Value())));
    min_preferred_logical_width_ =
        std::min(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     LayoutUnit(style_to_use.MaxWidth().Value())));
  }

  int to_add = BorderAndPaddingWidth().ToInt();
  min_preferred_logical_width_ += to_add;
  max_preferred_logical_width_ += to_add;

  ClearPreferredLogicalWidthsDirty();
}

PositionWithAffinity LayoutFileUploadControl::PositionForPoint(
    const LayoutPoint&) {
  return PositionWithAffinity();
}

HTMLInputElement* LayoutFileUploadControl::UploadButton() const {
  // FIXME: This should be on HTMLInputElement as an API like
  // innerButtonElement().
  HTMLInputElement* input = toHTMLInputElement(GetNode());
  Node* button_node = input->UserAgentShadowRoot()->firstChild();
  return isHTMLInputElement(button_node) ? toHTMLInputElement(button_node) : 0;
}

String LayoutFileUploadControl::ButtonValue() {
  if (HTMLInputElement* button = UploadButton())
    return button->value();

  return String();
}

String LayoutFileUploadControl::FileTextValue() const {
  HTMLInputElement* input = toHTMLInputElement(GetNode());
  DCHECK(input->files());
  return LayoutTheme::GetTheme().FileListNameForWidth(
      input->GetLocale(), input->files(), Style()->GetFont(),
      MaxFilenameWidth());
}

}  // namespace blink
