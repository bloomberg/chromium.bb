// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_TEST_UTILS_H_
#define UI_VIEWS_MUS_TEST_UTILS_H_

#include "ui/views/mus/window_manager_connection.h"

namespace views {
namespace test {

class WindowManagerConnectionTestApi {
 public:
  explicit WindowManagerConnectionTestApi(WindowManagerConnection* connection)
      : connection_(connection) {}
  ~WindowManagerConnectionTestApi() {}

  ui::Window* GetUiWindowAtScreenPoint(const gfx::Point& point) {
    return connection_->GetUiWindowAtScreenPoint(point);
  }

  ScreenMus* screen() { return connection_->screen_.get(); }

 private:
  WindowManagerConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnectionTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_MUS_TEST_UTILS_H_
