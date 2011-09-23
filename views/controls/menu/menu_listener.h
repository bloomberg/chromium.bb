// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_LISTENER_H_
#define VIEWS_CONTROLS_MENU_MENU_LISTENER_H_
#pragma once

#include "views/views_export.h"

namespace views {

// An interface for clients that want a notification when a menu is opened.
class VIEWS_EXPORT MenuListener {
 public:
  // This will be called after the menu has actually opened.
  virtual void OnMenuOpened() = 0;

 protected:
  virtual ~MenuListener() {}
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_LISTENER_H_
