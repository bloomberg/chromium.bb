// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/widget_example.h"

#include "base/utf_string_conversions.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"
#include "views/view.h"

namespace views {
namespace examples {
namespace {

// A layout manager that layouts a single child at
// the center of the host view.
class CenterLayout : public LayoutManager {
 public:
  CenterLayout() {}
  virtual ~CenterLayout() {}

  // Overridden from LayoutManager:
  virtual void Layout(View* host) {
    View* child = host->child_at(0);
    gfx::Size size = child->GetPreferredSize();
    child->SetBounds((host->width() - size.width()) / 2,
                     (host->height() - size.height()) / 2,
                     size.width(), size.height());
  }

  virtual gfx::Size GetPreferredSize(View* host) {
    return gfx::Size();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CenterLayout);
};

}  // namespace

WidgetExample::WidgetExample() : ExampleBase("Widget") {
}

WidgetExample::~WidgetExample() {
}

void WidgetExample::CreateExampleView(View* container) {
  container->SetLayoutManager(
      new BoxLayout(BoxLayout::kHorizontal, 0, 0, 2));
  BuildButton(container, "Create a popup widget", POPUP);
  BuildButton(container, "Create a transparent popup widget",
              TRANSPARENT_POPUP);
#if defined(OS_LINUX)
  View* vert_container = new View();
  container->AddChildView(vert_container);
  vert_container->SetLayoutManager(
      new BoxLayout(BoxLayout::kVertical, 0, 0, 20));
  BuildButton(vert_container, "Create a child widget", CHILD);
  BuildButton(vert_container, "Create a transparent child widget",
              TRANSPARENT_CHILD);
#endif
}

void WidgetExample::BuildButton(View* container,
                                const std::string& label,
                                int tag) {
  TextButton* button = new TextButton(this, ASCIIToUTF16(label));
  button->set_tag(tag);
  container->AddChildView(button);
}

void WidgetExample::InitWidget(Widget* widget, bool transparent) {
  // Add view/native buttons to close the popup widget.
  TextButton* close_button = new TextButton(
      this, ASCIIToUTF16("Close"));
  close_button->set_tag(CLOSE_WIDGET);
  // TODO(oshima): support transparent native view.
  NativeTextButton* native_button = new NativeTextButton(
      this, ASCIIToUTF16("Native Close"));
  native_button->set_tag(CLOSE_WIDGET);

  View* button_container = new View();
  button_container->SetLayoutManager(
      new BoxLayout(BoxLayout::kHorizontal, 0, 0, 1));
  button_container->AddChildView(close_button);
  button_container->AddChildView(native_button);

  View* widget_container = new View();
  widget_container->SetLayoutManager(new CenterLayout);
  widget_container->AddChildView(button_container);

  widget->SetContentsView(widget_container);

  if (!transparent) {
    widget_container->set_background(
        Background::CreateStandardPanelBackground());
  }

  // Show the widget.
  widget->Show();
}

#if defined(OS_LINUX)
void WidgetExample::CreateChild(View* parent, bool transparent) {
  Widget* widget = new Widget;
  // Compute where to place the child widget.
  // We'll place it at the center of the root widget.
  Widget* parent_widget = parent->GetWidget();
  gfx::Rect bounds = parent_widget->GetClientAreaScreenBounds();
  // Child widget is 200x200 square.
  bounds.SetRect((bounds.width() - 200) / 2, (bounds.height() - 200) / 2,
      200, 200);
  // Initialize the child widget with the computed bounds.
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.transparent = transparent;
  params.parent_widget = parent_widget;
  widget->Init(params);
  InitWidget(widget, transparent);
}
#endif

void WidgetExample::CreatePopup(View* parent, bool transparent) {
  Widget* widget = new Widget;

  // Compute where to place the popup widget.
  // We'll place it right below the create button.
  gfx::Point point = parent->GetMirroredPosition();
  // The position in point is relative to the parent. Make it absolute.
  View::ConvertPointToScreen(parent, &point);
  // Add the height of create_button_.
  point.Offset(0, parent->size().height());

  // Initialize the popup widget with the computed bounds.
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.transparent = transparent;
  params.parent_widget = parent->GetWidget();
  params.bounds = gfx::Rect(point.x(), point.y(), 200, 300);
  widget->Init(params);
  InitWidget(widget, transparent);
}

void WidgetExample::ButtonPressed(Button* sender, const Event& event) {
  switch (sender->tag()) {
    case POPUP:
      CreatePopup(sender, false);
      break;
    case TRANSPARENT_POPUP:
      CreatePopup(sender, true);
      break;
#if defined(OS_LINUX)
    case CHILD:
      CreateChild(sender, false);
      break;
    case TRANSPARENT_CHILD:
      CreateChild(sender, true);
      break;
#endif
    case CLOSE_WIDGET:
      sender->GetWidget()->Close();
      break;
  }
}

}  // namespace examples
}  // namespace views
