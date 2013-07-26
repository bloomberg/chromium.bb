// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase InputMethodBridgeTest;

namespace {

class TestInputMethod : public ui::DummyInputMethod {
 public:
  TestInputMethod();
  virtual ~TestInputMethod();

  virtual void SetFocusedTextInputClient(ui::TextInputClient* client) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() const OVERRIDE;

 private:
  ui::TextInputClient* text_input_client_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

TestInputMethod::TestInputMethod() : text_input_client_(NULL) {}

TestInputMethod::~TestInputMethod() {}

void TestInputMethod::SetFocusedTextInputClient(ui::TextInputClient* client) {
  // This simulates what the real InputMethod implementation does.
  if (text_input_client_)
    text_input_client_->GetTextInputType();
  text_input_client_ = client;
}

ui::TextInputClient* TestInputMethod::GetTextInputClient() const {
  return text_input_client_;
}

}  // namespace

TEST_F(InputMethodBridgeTest, DestructTest) {
  TestInputMethod input_method;
  GetContext()->SetProperty(aura::client::kRootWindowInputMethodKey,
                            static_cast<ui::InputMethod*>(&input_method));

  Widget* toplevel = new Widget;
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  // |child| owns |native_widget|.
  toplevel_params.native_widget = new DesktopNativeWidgetAura(toplevel);
  toplevel->Init(toplevel_params);

  Widget* child = new Widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  child_params.parent = toplevel->GetNativeView();
  // |child| owns |native_widget|.
  child_params.native_widget = new NativeWidgetAura(child);
  child->Init(child_params);

  child->GetInputMethod()->OnFocus();

  toplevel->CloseNow();

  GetContext()->SetProperty(aura::client::kRootWindowInputMethodKey,
                            static_cast<ui::InputMethod*>(NULL));
}

}  // namespace views
