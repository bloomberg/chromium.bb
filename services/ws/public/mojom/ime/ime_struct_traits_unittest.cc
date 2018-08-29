// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/mojom/ime/ime_struct_traits.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/public/mojom/ime/ime_struct_traits_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace ws {

namespace {

class IMEStructTraitsTest : public testing::Test,
                            public mojom::IMEStructTraitsTest {
 public:
  IMEStructTraitsTest() {}

 protected:
  mojom::IMEStructTraitsTestPtr GetTraitsTestProxy() {
    mojom::IMEStructTraitsTestPtr proxy;
    traits_test_bindings_.AddBinding(this, mojo::MakeRequest(&proxy));
    return proxy;
  }

 private:
  // mojom::IMEStructTraitsTest:
  void EchoTextInputMode(ui::TextInputMode in,
                         EchoTextInputModeCallback callback) override {
    std::move(callback).Run(in);
  }
  void EchoTextInputType(ui::TextInputType in,
                         EchoTextInputTypeCallback callback) override {
    std::move(callback).Run(in);
  }

  base::MessageLoop loop_;  // A MessageLoop is needed for Mojo IPC to work.
  mojo::BindingSet<mojom::IMEStructTraitsTest> traits_test_bindings_;

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

TEST_F(IMEStructTraitsTest, TextInputMode) {
  const ui::TextInputMode kTextInputModes[] = {
      ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_MODE_NONE,
      ui::TEXT_INPUT_MODE_TEXT,    ui::TEXT_INPUT_MODE_TEL,
      ui::TEXT_INPUT_MODE_URL,     ui::TEXT_INPUT_MODE_EMAIL,
      ui::TEXT_INPUT_MODE_NUMERIC, ui::TEXT_INPUT_MODE_DECIMAL,
      ui::TEXT_INPUT_MODE_SEARCH,
  };

  mojom::IMEStructTraitsTestPtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTextInputModes); i++) {
    ui::TextInputMode mode_out;
    ASSERT_TRUE(proxy->EchoTextInputMode(kTextInputModes[i], &mode_out));
    EXPECT_EQ(kTextInputModes[i], mode_out);
  }
}

TEST_F(IMEStructTraitsTest, TextInputType) {
  const ui::TextInputType kTextInputTypes[] = {
      ui::TEXT_INPUT_TYPE_NONE,
      ui::TEXT_INPUT_TYPE_TEXT,
      ui::TEXT_INPUT_TYPE_PASSWORD,
      ui::TEXT_INPUT_TYPE_SEARCH,
      ui::TEXT_INPUT_TYPE_EMAIL,
      ui::TEXT_INPUT_TYPE_NUMBER,
      ui::TEXT_INPUT_TYPE_TELEPHONE,
      ui::TEXT_INPUT_TYPE_URL,
      ui::TEXT_INPUT_TYPE_DATE,
      ui::TEXT_INPUT_TYPE_DATE_TIME,
      ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
      ui::TEXT_INPUT_TYPE_MONTH,
      ui::TEXT_INPUT_TYPE_TIME,
      ui::TEXT_INPUT_TYPE_WEEK,
      ui::TEXT_INPUT_TYPE_TEXT_AREA,
      ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE,
      ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD,
  };

  mojom::IMEStructTraitsTestPtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTextInputTypes); i++) {
    ui::TextInputType type_out;
    ASSERT_TRUE(proxy->EchoTextInputType(kTextInputTypes[i], &type_out));
    EXPECT_EQ(kTextInputTypes[i], type_out);
  }
}

}  // namespace ws
