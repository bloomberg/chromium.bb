// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_VIEW_CLICK_LISTENER_H_
#define ASH_SYSTEM_TRAY_VIEW_CLICK_LISTENER_H_

#include "ash/ash_export.h"

namespace views {
class View;
}

namespace ash {

class ASH_EXPORT ViewClickListener {
 public:
  virtual void OnViewClicked(views::View* sender) = 0;

 protected:
  virtual ~ViewClickListener() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_VIEW_CLICK_LISTENER_H_
