// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_aura.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace corewm {

typedef ViewsTestBase TooltipAuraTest;

// TODO(sky): clean this up.
gfx::Font GetDefaultFont() {
  return ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont);
}

TEST_F(TooltipAuraTest, TrimTooltipToFitTests) {
  const int max_width = 4000;
  string16 tooltip;
  int width, line_count, expect_lines;
  int max_pixel_width = 400;  // copied from constants in tooltip_controller.cc
  int max_lines = 10;  // copied from constants in tooltip_controller.cc
  gfx::Font font = GetDefaultFont();
  size_t tooltip_len;

  // Error in computed size vs. expected size should not be greater than the
  // size of the longest word.
  int error_in_pixel_width = font.GetStringWidth(ASCIIToUTF16("tooltip"));

  // Long tooltips should wrap to next line
  tooltip.clear();
  width = line_count = -1;
  expect_lines = 3;
  for (; font.GetStringWidth(tooltip) <= (expect_lines - 1) * max_pixel_width;)
    tooltip.append(ASCIIToUTF16("This is part of the tooltip"));
  tooltip_len = tooltip.length();
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  EXPECT_NEAR(max_pixel_width, width, error_in_pixel_width);
  EXPECT_EQ(expect_lines, line_count);
  EXPECT_EQ(tooltip_len + expect_lines - 1, tooltip.length());

  // More than |max_lines| lines should get truncated at 10 lines.
  tooltip.clear();
  width = line_count = -1;
  expect_lines = 13;
  for (; font.GetStringWidth(tooltip) <= (expect_lines - 1) * max_pixel_width;)
    tooltip.append(ASCIIToUTF16("This is part of the tooltip"));
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  EXPECT_NEAR(max_pixel_width, width, error_in_pixel_width);
  EXPECT_EQ(max_lines, line_count);

  // Long multi line tooltips should wrap individual lines.
  tooltip.clear();
  width = line_count = -1;
  expect_lines = 4;
  for (; font.GetStringWidth(tooltip) <= (expect_lines - 2) * max_pixel_width;)
    tooltip.append(ASCIIToUTF16("This is part of the tooltip"));
  tooltip.insert(tooltip.length() / 2, ASCIIToUTF16("\n"));
  tooltip_len = tooltip.length();
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  EXPECT_NEAR(max_pixel_width, width, error_in_pixel_width);
  EXPECT_EQ(expect_lines, line_count);
  // We may have inserted the line break above near a space which will get
  // trimmed. Hence we may be off by 1 in the final tooltip length calculation.
  EXPECT_NEAR(tooltip_len + expect_lines - 2, tooltip.length(), 1);

#if !defined(OS_WIN)
  // Tooltip with really long word gets elided.
  tooltip.clear();
  width = line_count = -1;
  tooltip = UTF8ToUTF16(std::string('a', max_pixel_width));
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  EXPECT_NEAR(max_pixel_width, width, 5);
  EXPECT_EQ(1, line_count);
  EXPECT_EQ(gfx::ElideText(UTF8ToUTF16(std::string('a', max_pixel_width)), font,
                          max_pixel_width, gfx::ELIDE_AT_END), tooltip);
#endif

  // Normal small tooltip should stay as is.
  tooltip.clear();
  width = line_count = -1;
  tooltip = ASCIIToUTF16("Small Tooltip");
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  EXPECT_EQ(font.GetStringWidth(ASCIIToUTF16("Small Tooltip")), width);
  EXPECT_EQ(1, line_count);
  EXPECT_EQ(ASCIIToUTF16("Small Tooltip"), tooltip);

  // Normal small multi-line tooltip should stay as is.
  tooltip.clear();
  width = line_count = -1;
  tooltip = ASCIIToUTF16("Multi line\nTooltip");
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  int expected_width = font.GetStringWidth(ASCIIToUTF16("Multi line"));
  expected_width = std::max(expected_width,
                            font.GetStringWidth(ASCIIToUTF16("Tooltip")));
  EXPECT_EQ(expected_width, width);
  EXPECT_EQ(2, line_count);
  EXPECT_EQ(ASCIIToUTF16("Multi line\nTooltip"), tooltip);

  // Whitespaces in tooltips are preserved.
  tooltip.clear();
  width = line_count = -1;
  tooltip = ASCIIToUTF16("Small  Tool  t\tip");
  TooltipAura::TrimTooltipToFit(max_width, &tooltip, &width, &line_count);
  EXPECT_EQ(font.GetStringWidth(ASCIIToUTF16("Small  Tool  t\tip")), width);
  EXPECT_EQ(1, line_count);
  EXPECT_EQ(ASCIIToUTF16("Small  Tool  t\tip"), tooltip);
}

}  // namespace corewm
}  // namespace views
