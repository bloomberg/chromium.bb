// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/mojom/ime/ime_struct_traits.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace ws {

namespace {

class IMEStructTraitsTest : public testing::Test {
 public:
  IMEStructTraitsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IMEStructTraitsTest);
};

}  // namespace

TEST_F(IMEStructTraitsTest, CandidateWindowProperties) {
  ui::CandidateWindow::CandidateWindowProperty input;
  input.page_size = 7;
  input.cursor_position = 3;
  input.is_cursor_visible = true;
  input.is_vertical = false;
  input.show_window_at_composition = true;
  input.auxiliary_text = "abcdefghij";
  input.is_auxiliary_text_visible = false;

  ui::CandidateWindow::CandidateWindowProperty output;
  EXPECT_TRUE(mojom::CandidateWindowProperties::Deserialize(
      mojom::CandidateWindowProperties::Serialize(&input), &output));

  EXPECT_EQ(input.page_size, output.page_size);
  EXPECT_EQ(input.cursor_position, output.cursor_position);
  EXPECT_EQ(input.is_cursor_visible, output.is_cursor_visible);
  EXPECT_EQ(input.is_vertical, output.is_vertical);
  EXPECT_EQ(input.show_window_at_composition,
            output.show_window_at_composition);
  EXPECT_EQ(input.auxiliary_text, output.auxiliary_text);
  EXPECT_EQ(input.is_auxiliary_text_visible, output.is_auxiliary_text_visible);

  // Reverse boolean fields and check we still serialize/deserialize correctly.
  input.is_cursor_visible = !input.is_cursor_visible;
  input.is_vertical = !input.is_vertical;
  input.show_window_at_composition = !input.show_window_at_composition;
  input.is_auxiliary_text_visible = !input.is_auxiliary_text_visible;
  EXPECT_TRUE(mojom::CandidateWindowProperties::Deserialize(
      mojom::CandidateWindowProperties::Serialize(&input), &output));

  EXPECT_EQ(input.is_cursor_visible, output.is_cursor_visible);
  EXPECT_EQ(input.is_vertical, output.is_vertical);
  EXPECT_EQ(input.show_window_at_composition,
            output.show_window_at_composition);
  EXPECT_EQ(input.is_auxiliary_text_visible, output.is_auxiliary_text_visible);
}

TEST_F(IMEStructTraitsTest, CandidateWindowEntry) {
  ui::CandidateWindow::Entry input;
  input.value = base::UTF8ToUTF16("entry_value");
  input.label = base::UTF8ToUTF16("entry_label");
  input.annotation = base::UTF8ToUTF16("entry_annotation");
  input.description_title = base::UTF8ToUTF16("entry_description_title");
  input.description_body = base::UTF8ToUTF16("entry_description_body");

  ui::CandidateWindow::Entry output;
  EXPECT_TRUE(mojom::CandidateWindowEntry::Deserialize(
      mojom::CandidateWindowEntry::Serialize(&input), &output));

  EXPECT_EQ(input.value, output.value);
  EXPECT_EQ(input.label, output.label);
  EXPECT_EQ(input.annotation, output.annotation);
  EXPECT_EQ(input.description_title, output.description_title);
  EXPECT_EQ(input.description_body, output.description_body);
}

TEST_F(IMEStructTraitsTest, CompositionText) {
  ui::CompositionText input;
  input.text = base::UTF8ToUTF16("abcdefghij");
  ui::ImeTextSpan ime_text_span_1(0, 2, ui::ImeTextSpan::Thickness::kThin);
  ime_text_span_1.underline_color = SK_ColorGRAY;
  input.ime_text_spans.push_back(ime_text_span_1);
  ui::ImeTextSpan ime_text_span_2(ui::ImeTextSpan::Type::kComposition, 3, 6,
                                  ui::ImeTextSpan::Thickness::kThick,
                                  SK_ColorGREEN);
  ime_text_span_2.underline_color = SK_ColorRED;
  input.ime_text_spans.push_back(ime_text_span_2);
  input.selection = gfx::Range(1, 7);

  ui::CompositionText output;
  EXPECT_TRUE(mojom::CompositionText::Deserialize(
      mojom::CompositionText::Serialize(&input), &output));

  EXPECT_EQ(input, output);
}

}  // namespace ws
