// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/example_base.h"

#include <stdarg.h>
#include <string>

#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"

namespace examples {

// Prints a message in the status area, at the bottom of the window.
void ExampleBase::PrintStatus(const wchar_t* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::wstring msg;
  StringAppendV(&msg, format, ap);
  main_->SetStatus(msg);
}

}  // namespace examples
