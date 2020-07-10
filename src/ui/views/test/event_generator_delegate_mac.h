// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_EVENT_GENERATOR_DELEGATE_MAC_H_
#define UI_VIEWS_TEST_EVENT_GENERATOR_DELEGATE_MAC_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace test {
class EventGenerator;
class EventGeneratorDelegate;
}  // namespace test
}  // namespace ui

namespace views {
namespace test {

std::unique_ptr<ui::test::EventGeneratorDelegate>
CreateEventGeneratorDelegateMac(ui::test::EventGenerator* owner,
                                gfx::NativeWindow root_window,
                                gfx::NativeWindow window);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_EVENT_GENERATOR_DELEGATE_MAC_H_
