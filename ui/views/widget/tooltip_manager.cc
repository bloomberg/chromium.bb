// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager.h"

#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

// Maximum number of lines we allow in the tooltip.
const size_t kMaxLines = 6;

namespace views {

// static
void TooltipManager::TrimTooltipToFit(string16* text,
                                      int* max_width,
                                      int* line_count,
                                      int x,
                                      int y,
                                      gfx::NativeView context) {
  *max_width = 0;
  *line_count = 0;

  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip.
  if (text->length() > kMaxTooltipLength)
    *text = text->substr(0, kMaxTooltipLength);

  // Determine the available width for the tooltip.
  int available_width = GetMaxWidth(x, y, context);

  // Split the string into at most kMaxLines lines.
  std::vector<string16> lines;
  base::SplitString(*text, '\n', &lines);
  if (lines.size() > kMaxLines)
    lines.resize(kMaxLines);
  *line_count = static_cast<int>(lines.size());

  // Format each line to fit.
  const gfx::FontList& font_list = GetDefaultFontList();
  string16 result;
  for (std::vector<string16>::iterator i = lines.begin(); i != lines.end();
       ++i) {
    string16 elided_text =
        gfx::ElideText(*i, font_list, available_width, gfx::ELIDE_AT_END);
    *max_width = std::max(*max_width,
                          gfx::GetStringWidth(elided_text, font_list));
    if (!result.empty())
      result.push_back('\n');
    result.append(elided_text);
  }
  *text = result;
}

}  // namespace views
