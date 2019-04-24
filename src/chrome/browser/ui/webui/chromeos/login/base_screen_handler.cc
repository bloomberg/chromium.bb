// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

BaseScreenHandler::BaseScreenHandler(OobeScreen oobe_screen,
                                     JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container), oobe_screen_(oobe_screen) {}

BaseScreenHandler::~BaseScreenHandler() {}

}  // namespace chromeos
