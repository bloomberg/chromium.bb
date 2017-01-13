// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/ime/ime_struct_traits_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/composition_underline.h"

namespace ui {

namespace {

class IMEStructTraitsTest : public testing::Test,
                            public mojom::IMEStructTraitsTest {
 public:
  IMEStructTraitsTest() {}

 protected:
  mojom::IMEStructTraitsTestPtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // mojom::IMEStructTraitsTest:
  void EchoCompositionText(
      const CompositionText& in,
      const EchoCompositionTextCallback& callback) override {
    callback.Run(in);
  }
  void EchoTextInputMode(TextInputMode in,
                         const EchoTextInputModeCallback& callback) override {
    callback.Run(in);
  }
  void EchoTextInputType(TextInputType in,
                         const EchoTextInputTypeCallback& callback) override {
    callback.Run(in);
  }

  base::MessageLoop loop_;  // A MessageLoop is needed for Mojo IPC to work.
  mojo::BindingSet<mojom::IMEStructTraitsTest> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(IMEStructTraitsTest);
};

}  // namespace

TEST_F(IMEStructTraitsTest, CompositionText) {
  CompositionText input;
  input.text = base::UTF8ToUTF16("abcdefghij");
  input.underlines.push_back(CompositionUnderline(0, 2, SK_ColorGRAY, false));
  input.underlines.push_back(
      CompositionUnderline(3, 6, SK_ColorRED, true, SK_ColorGREEN));
  input.selection = gfx::Range(1, 7);

  mojom::IMEStructTraitsTestPtr proxy = GetTraitsTestProxy();
  CompositionText output;
  proxy->EchoCompositionText(input, &output);

  EXPECT_EQ(input, output);
}

TEST_F(IMEStructTraitsTest, TextInputMode) {
  const TextInputMode kTextInputModes[] = {
      TEXT_INPUT_MODE_DEFAULT,     TEXT_INPUT_MODE_VERBATIM,
      TEXT_INPUT_MODE_LATIN,       TEXT_INPUT_MODE_LATIN_NAME,
      TEXT_INPUT_MODE_LATIN_PROSE, TEXT_INPUT_MODE_FULL_WIDTH_LATIN,
      TEXT_INPUT_MODE_KANA,        TEXT_INPUT_MODE_KANA_NAME,
      TEXT_INPUT_MODE_KATAKANA,    TEXT_INPUT_MODE_NUMERIC,
      TEXT_INPUT_MODE_TEL,         TEXT_INPUT_MODE_EMAIL,
      TEXT_INPUT_MODE_URL,
  };

  mojom::IMEStructTraitsTestPtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTextInputModes); i++) {
    ui::TextInputMode mode_out;
    ASSERT_TRUE(proxy->EchoTextInputMode(kTextInputModes[i], &mode_out));
    EXPECT_EQ(kTextInputModes[i], mode_out);
  }
}

TEST_F(IMEStructTraitsTest, TextInputType) {
  const TextInputType kTextInputTypes[] = {
      TEXT_INPUT_TYPE_NONE,
      TEXT_INPUT_TYPE_TEXT,
      TEXT_INPUT_TYPE_PASSWORD,
      TEXT_INPUT_TYPE_SEARCH,
      TEXT_INPUT_TYPE_EMAIL,
      TEXT_INPUT_TYPE_NUMBER,
      TEXT_INPUT_TYPE_TELEPHONE,
      TEXT_INPUT_TYPE_URL,
      TEXT_INPUT_TYPE_DATE,
      TEXT_INPUT_TYPE_DATE_TIME,
      TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
      TEXT_INPUT_TYPE_MONTH,
      TEXT_INPUT_TYPE_TIME,
      TEXT_INPUT_TYPE_WEEK,
      TEXT_INPUT_TYPE_TEXT_AREA,
      TEXT_INPUT_TYPE_CONTENT_EDITABLE,
      TEXT_INPUT_TYPE_DATE_TIME_FIELD,
  };

  mojom::IMEStructTraitsTestPtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTextInputTypes); i++) {
    ui::TextInputType type_out;
    ASSERT_TRUE(proxy->EchoTextInputType(kTextInputTypes[i], &type_out));
    EXPECT_EQ(kTextInputTypes[i], type_out);
  }
}

}  // namespace ui
