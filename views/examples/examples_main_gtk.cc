// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_gtk.h"
#include "views/examples/examples_main_base.h"

namespace {

class ExamplesMainGtk : public examples::ExamplesMainBase {
 public:
  ExamplesMainGtk() {}
  virtual ~ExamplesMainGtk() {}

  // Overrides ExamplesMainBase.
  virtual views::Widget* CreateTopLevelWidget() {
    return new views::WidgetGtk(views::WidgetGtk::TYPE_DECORATED_WINDOW);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExamplesMainGtk);
};

}  // namespace

int main(int argc, char** argv) {
  // Initializes gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);

  ExamplesMainGtk main;
  main.Run();
  return 0;
}

