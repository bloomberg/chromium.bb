// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/widget_example.h"

#include "base/utf_string_conversions.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
namespace examples {

namespace {

class DialogExample : public DialogDelegateView {
 public:
  virtual string16 GetWindowTitle() const OVERRIDE;
};

string16 DialogExample::GetWindowTitle() const {
  return ASCIIToUTF16("Dialog Widget Example");
}

}  // namespace

WidgetExample::WidgetExample() : ExampleBase("Widget") {
}

WidgetExample::~WidgetExample() {
}

void WidgetExample::CreateExampleView(View* container) {
  container->SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0, 2));
  BuildButton(container, "Popup widget", POPUP);
  BuildButton(container, "Dialog widget", DIALOG);
#if defined(OS_LINUX)
  // Windows does not support TYPE_CONTROL top-level widgets.
  BuildButton(container, "Child widget", CHILD);
#endif
}

void WidgetExample::BuildButton(View* container,
                                const std::string& label,
                                int tag) {
  TextButton* button = new TextButton(this, ASCIIToUTF16(label));
  button->set_focusable(true);
  button->set_tag(tag);
  container->AddChildView(button);
}

void WidgetExample::ShowWidget(View* sender, Widget::InitParams params) {
  // Setup shared Widget heirarchy and bounds parameters.
  params.parent = sender->GetWidget()->GetNativeView();
  params.bounds = gfx::Rect(sender->GetBoundsInScreen().CenterPoint(),
                            gfx::Size(300, 200));

  Widget* widget = new Widget();
  widget->Init(params);

  // If the Widget has no contents by default, add a view with a 'Close' button.
  if (!widget->GetContentsView()) {
    View* contents = new View();
    contents->SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0));
    contents->set_background(Background::CreateSolidBackground(SK_ColorGRAY));
    BuildButton(contents, "Close", CLOSE_WIDGET);
    widget->SetContentsView(contents);
  }

  widget->Show();
}

void WidgetExample::ButtonPressed(Button* sender, const ui::Event& event) {
  switch (sender->tag()) {
    case POPUP:
      ShowWidget(sender, Widget::InitParams(Widget::InitParams::TYPE_POPUP));
      break;
    case DIALOG: {
      Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
      params.delegate = new DialogExample();
      params.remove_standard_frame = true;
      params.transparent = true;
      ShowWidget(sender, params);
      break;
    }
    case CHILD:
      ShowWidget(sender, Widget::InitParams(Widget::InitParams::TYPE_CONTROL));
      break;
    case CLOSE_WIDGET:
      sender->GetWidget()->Close();
      break;
  }
}

}  // namespace examples
}  // namespace views
