// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_COLOR_MODE_OBSERVER_H_
#define ASH_PUBLIC_CPP_STYLE_COLOR_MODE_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_PUBLIC_EXPORT ColorModeObserver : public base::CheckedObserver {
 public:
  // Called when the color mode changes.
  virtual void OnColorModeChanged(bool dark_mode_enabled) {}

  // Called when the themed state of the color mode is changed.
  virtual void OnColorModeThemed(bool is_themed) {}

 protected:
  ~ColorModeObserver() override = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_COLOR_MODE_OBSERVER_H_
