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

}  // namespace ui
