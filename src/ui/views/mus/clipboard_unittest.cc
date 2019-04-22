// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojo/clipboard_client.h"

#include "ui/events/platform/platform_event_source.h"
#include "ui/views/test/views_interactive_ui_test_base.h"

namespace ui {

namespace {

views::ViewsTestBase* g_test_base = nullptr;

// This class is necessary to allow the Mus version of ClipboardTest to
// initialize itself as if it's a ViewsTestBase (which creates the MusClient and
// does other necessary setup). TODO(crbug/917180): improve this.
class ViewsTestBaseNoTest : public views::ViewsInteractiveUITestBase {
 public:
  ViewsTestBaseNoTest() = default;
  ~ViewsTestBaseNoTest() override = default;

  // views::ViewsInteractiveUITestBase:
  void TestBody() override {}
};

}  // namespace

struct PlatformClipboardTraits {
  static std::unique_ptr<PlatformEventSource> GetEventSource() {
    return nullptr;
  }

  static Clipboard* Create() {
    g_test_base = new ViewsTestBaseNoTest();
    g_test_base->SetUp();

    return Clipboard::GetForCurrentThread();
  }

  static void Destroy(Clipboard* clipboard) {
    g_test_base->TearDown();
    g_test_base = nullptr;
  }
};

class MusClipboardTestName {
 public:
  template <typename T>
  static std::string GetName(int index) {
    return "MusClipboardTest";
  }
};

using TypesToTest = PlatformClipboardTraits;
using NamesOfTypesToTest = MusClipboardTestName;

}  // namespace ui

#include "ui/base/clipboard/clipboard_test_template.h"
