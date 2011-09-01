// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/example_base.h"

#include <stdarg.h>
#include <string>

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "views/controls/button/text_button.h"
#include "views/examples/examples_main.h"

#if defined(OS_CHROMEOS)
#include "views/controls/menu/native_menu_gtk.h"
#endif

namespace {

using views::View;

// Some of GTK based view classes require NativeWidgetGtk in the view
// parent chain. This class is used to defer the creation of such
// views until a NativeWidgetGtk is added to the view hierarchy.
class ContainerView : public View {
 public:
  explicit ContainerView(examples::ExampleBase* base)
      : example_view_created_(false),
        example_base_(base) {
  }

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child) {
    View::ViewHierarchyChanged(is_add, parent, child);
    // We're not using child == this because a Widget may not be
    // availalbe when this is added to the hierarchy.
    if (is_add && GetWidget() && !example_view_created_) {
      example_view_created_ = true;
      example_base_->CreateExampleView(this);
    }
  }

 private:
  // true if the example view has already been created, or false otherwise.
  bool example_view_created_;

  examples::ExampleBase* example_base_;

  DISALLOW_COPY_AND_ASSIGN(ContainerView);
};

}  // namespace

namespace examples {

ExampleBase::ExampleBase(ExamplesMain* main)
    : main_(main) {
  container_ = new ContainerView(this);
}

// Prints a message in the status area, at the bottom of the window.
void ExampleBase::PrintStatus(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string msg;
  base::StringAppendV(&msg, format, ap);
  main_->SetStatus(msg);
}

}  // namespace examples
