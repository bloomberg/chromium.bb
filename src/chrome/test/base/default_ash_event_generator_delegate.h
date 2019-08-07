// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DEFAULT_ASH_EVENT_GENERATOR_DELEGATE_H_
#define CHROME_TEST_BASE_DEFAULT_ASH_EVENT_GENERATOR_DELEGATE_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace test {
class EventGenerator;
class EventGeneratorDelegate;
}  // namespace test
}  // namespace ui

std::unique_ptr<ui::test::EventGeneratorDelegate>
CreateAshEventGeneratorDelegate(ui::test::EventGenerator* owner,
                                gfx::NativeWindow root_window,
                                gfx::NativeWindow window);

#endif  // CHROME_TEST_BASE_DEFAULT_ASH_EVENT_GENERATOR_DELEGATE_H_
