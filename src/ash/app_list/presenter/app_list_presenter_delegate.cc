// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/presenter/app_list_presenter_delegate.h"

#include "ash/app_list/views/app_list_view.h"
#include "base/time/time.h"

namespace app_list {

base::TimeDelta AppListPresenterDelegate::GetAnimationDurationFullscreen(
    bool is_side_shelf,
    bool is_fullscreen) {
  // Duration for the hide animation for the fullscreen app list while in side
  // shelf.
  static constexpr base::TimeDelta kAnimationDurationSideShelfFullscreenMs =
      base::TimeDelta::FromMilliseconds(150);

  // Duration for the hide animation for the app list when it is not starting
  // from a fullscreen state.
  static constexpr base::TimeDelta kAnimationDurationMs =
      base::TimeDelta::FromMilliseconds(200);

  // Duration for the hide animation for the app list when it is starting from a
  // fullscreen state.
  static constexpr base::TimeDelta kAnimationDurationFromFullscreenMs =
      base::TimeDelta::FromMilliseconds(250);

  if (is_side_shelf)
    return kAnimationDurationSideShelfFullscreenMs;

  return is_fullscreen ? kAnimationDurationFromFullscreenMs
                       : kAnimationDurationMs;
}


}  // namespace app_list
