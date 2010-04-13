// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#define VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_

#include "views/background.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/fill_layout.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

namespace {

// A layout manager that layouts child views vertically.
// TODO(oshima): support horizontal support and
// move to views/.
class BoxLayout : public views::LayoutManager {
 public:
  BoxLayout() {}
  virtual ~BoxLayout() {
  }

  // Overridden from LayoutManager:
  virtual void Layout(views::View* host) {
    int height = host->height();
    int width = host->width();
    int count = host->GetChildViewCount();

    int y = 0;
    for (int i = 0; i < count; i++) {
      views::View* child = host->GetChildViewAt(i);
      child->SetBounds(0, y, width, height / count);
      y = height * (i + 1)/ count;
    }
  }

  virtual gfx::Size GetPreferredSize(views::View* host) {
    return host->GetPreferredSize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoxLayout);
};

// A layout manager that layouts a single child at
// the center of the host view.
class CenterLayout : public views::LayoutManager {
 public:
  CenterLayout() {
  }
  virtual ~CenterLayout() {
  }

  // Overridden from LayoutManager:
  virtual void Layout(views::View* host) {
    views::View* child = host->GetChildViewAt(0);
    gfx::Size size = child->GetPreferredSize();
    child->SetBounds((host->width() - size.width()) / 2,
                     (host->height() - size.height()) / 2,
                     size.width(), size.height());
  }

  virtual gfx::Size GetPreferredSize(views::View* host) {
    return host->GetPreferredSize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CenterLayout);
};

}  // namespace

namespace examples {

using views::Widget;

// WidgetExample demonstrates how to create a popup widget.
class WidgetExample : public ExampleBase, public views::ButtonListener {
 public:
  explicit WidgetExample(ExamplesMain* main) : ExampleBase(main) {
  }

  virtual ~WidgetExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Widget";
  }

  virtual void CreateExampleView(views::View* container) {
    container->SetLayoutManager(new BoxLayout());
    BuildButton(container, L"Create a popup widget", POPUP);
    BuildButton(container, L"Create a transparent popup widget",
                TRANSPARENT_POPUP);
#if defined(OS_LINUX)
    BuildButton(container, L"Create a child widget", CHILD);
    BuildButton(container,
                L"Create a transparent child widget", TRANSPARENT_CHILD);
#endif
  }

 private:
  enum Command {
    POPUP,
    CHILD,
    TRANSPARENT_POPUP,
    TRANSPARENT_CHILD,
    CLOSE_WIDGET,
  };

  void BuildButton(views::View* container, const std::wstring& label, int tag) {
    views::TextButton* button = new views::TextButton(this, label);
    button->set_tag(tag);
    container->AddChildView(button);
  }

  void InitWidget(Widget* widget,
                  const Widget::TransparencyParam transparency) {
    // Add a button to close the popup widget.
    views::TextButton* close_button = new views::TextButton(this, L"Close");
    close_button->set_tag(CLOSE_WIDGET);
    views::View* widget_container = new views::View();
    widget_container->SetLayoutManager(new CenterLayout);
    if (transparency == Widget::Transparent)
      close_button->set_background(
          views::Background::CreateStandardPanelBackground());
    else
      widget_container->set_background(
          views::Background::CreateStandardPanelBackground());
    widget_container->AddChildView(close_button);
    widget->GetRootView()->SetContentsView(widget_container);

    // Show the widget.
    widget->Show();
  }

#if defined(OS_LINUX)
  void CreateChild(views::View* parent,
                   const Widget::TransparencyParam transparency) {
    views::WidgetGtk* widget =
        new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
    if (transparency == Widget::Transparent)
      widget->MakeTransparent();
    // Compute where to place the child widget.
    // We'll place it at the center of the root widget.
    views::WidgetGtk* parent_widget =
        static_cast<views::WidgetGtk*>(parent->GetWidget()->GetRootWidget());
    gfx::Rect bounds;
    parent_widget->GetBounds(&bounds, false);
    // Child widget is 200x200 square.
    bounds.SetRect((bounds.width() - 200) / 2, (bounds.height() - 200) / 2,
                   200, 200);
    // Initialize the child widget with the computed bounds.
    widget->Init(parent_widget->window_contents(), bounds);
    InitWidget(widget, transparency);
  }
#endif

  void CreatePopup(views::View* parent,
                   const Widget::TransparencyParam transparency) {
    Widget* widget = Widget::CreatePopupWidget(transparency,
                                               Widget::AcceptEvents,
                                               Widget::DeleteOnDestroy);
    // Compute where to place the popup widget.
    // We'll place it right below the create button.
    gfx::Point point = parent->GetPosition();
    // The position in point is relative to the parent. Make it absolute.
    views::View::ConvertPointToScreen(parent, &point);
    // Add the height of create_button_.
    point.Offset(0, parent->size().height());
    gfx::Rect bounds(point.x(), point.y(), 200, 300);
    // Initialize the popup widget with the computed bounds.
    widget->Init(NULL, bounds);
    InitWidget(widget, transparency);
  }

  // ButtonListner implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    switch (sender->tag()) {
      case POPUP:
        CreatePopup(sender, Widget::NotTransparent);
        break;
      case TRANSPARENT_POPUP:
        CreatePopup(sender, Widget::Transparent);
        break;
#if defined(OS_LINUX)
      case CHILD:
        CreateChild(sender, Widget::NotTransparent);
        break;
      case TRANSPARENT_CHILD:
        CreateChild(sender, Widget::Transparent);
        break;
#endif
      case CLOSE_WIDGET:
        sender->GetWidget()->Close();
        break;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(WidgetExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
