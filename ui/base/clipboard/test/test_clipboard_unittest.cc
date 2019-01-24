// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/test/test_clipboard.h"

#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/events/platform/platform_event_source.h"
#endif

namespace ui {

namespace {

base::test::ScopedTaskEnvironment* g_task_environment = nullptr;

}  // namespace

struct TestClipboardTraits {
#if defined(USE_AURA)
  static std::unique_ptr<PlatformEventSource> GetEventSource() {
    return nullptr;
  }
#endif

  static Clipboard* Create() {
    DCHECK(!g_task_environment);
    g_task_environment = new base::test::ScopedTaskEnvironment(
        base::test::ScopedTaskEnvironment::MainThreadType::UI);
    return TestClipboard::CreateForCurrentThread();
  }

  static void Destroy(Clipboard* clipboard) {
    ASSERT_EQ(Clipboard::GetForCurrentThread(), clipboard);
    Clipboard::DestroyClipboardForCurrentThread();

    delete g_task_environment;
    g_task_environment = nullptr;
  }
};

class TestClipboardTestName {
 public:
  template <typename T>
  static std::string GetName(int index) {
    return "TestClipboardTest";
  }
};

using TypesToTest = TestClipboardTraits;
using NamesOfTypesToTest = TestClipboardTestName;

}  // namespace ui

#include "ui/base/clipboard/clipboard_test_template.h"
