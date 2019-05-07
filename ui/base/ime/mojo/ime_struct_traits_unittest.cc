// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojo/ime_types_struct_traits.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/mojo/ime_struct_traits_test.mojom.h"
#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace ui {

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

TEST_F(IMEStructTraitsTest, TextInputMode) {
  const ui::TextInputMode kTextInputModes[] = {
      ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_MODE_NONE,
      ui::TEXT_INPUT_MODE_TEXT,    ui::TEXT_INPUT_MODE_TEL,
      ui::TEXT_INPUT_MODE_URL,     ui::TEXT_INPUT_MODE_EMAIL,
      ui::TEXT_INPUT_MODE_NUMERIC, ui::TEXT_INPUT_MODE_DECIMAL,
      ui::TEXT_INPUT_MODE_SEARCH,
  };

  mojom::IMEStructTraitsTestPtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < base::size(kTextInputModes); i++) {
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
  for (size_t i = 0; i < base::size(kTextInputTypes); i++) {
    ui::TextInputType type_out;
    ASSERT_TRUE(proxy->EchoTextInputType(kTextInputTypes[i], &type_out));
    EXPECT_EQ(kTextInputTypes[i], type_out);
  }
}

TEST_F(IMEStructTraitsTest, CompositionText) {
  ui::CompositionText input;
  input.text = base::UTF8ToUTF16("abcdefghij");
  ui::ImeTextSpan ime_text_span_1(ui::ImeTextSpan::Type::kComposition, 0, 2,
                                  ui::ImeTextSpan::Thickness::kThin);
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

}  // namespace ui
