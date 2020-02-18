// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TAB_COUNTER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TAB_COUNTER_BUTTON_H_

#include "ui/views/controls/button/button.h"

class TabStripModel;

std::unique_ptr<views::View> CreateWebUITabCounterButton(
    views::ButtonListener* listener,
    TabStripModel* tab_strip_model);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TAB_COUNTER_BUTTON_H_
