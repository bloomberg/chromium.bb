// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_pump_dispatcher.h"

namespace views {
namespace internal {

MenuMessagePumpDispatcher::MenuMessagePumpDispatcher(MenuController* controller)
    : menu_controller_(controller) {}

MenuMessagePumpDispatcher::~MenuMessagePumpDispatcher() {}

}  // namespace internal
}  // namespace views
