// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_MUS_CLIENT_TEST_API_H_
#define UI_VIEWS_MUS_MUS_CLIENT_TEST_API_H_

#include "base/macros.h"
#include "ui/views/mus/mus_client.h"

namespace views {

class MusClientTestApi {
 public:
  static ScreenMus* screen() { return MusClient::Get()->screen_.get(); }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MusClientTestApi);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_MUS_CLIENT_TEST_API_H_
