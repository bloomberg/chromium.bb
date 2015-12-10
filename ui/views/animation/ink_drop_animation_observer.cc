// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_observer.h"

#include <string>

#include "base/logging.h"

namespace views {

std::string ToString(
    InkDropAnimationObserver::InkDropAnimationEndedReason reason) {
  switch (reason) {
    case InkDropAnimationObserver::SUCCESS:
      return std::string("SUCCESS");
    case InkDropAnimationObserver::PRE_EMPTED:
      return std::string("PRE_EMPTED");
  }
  NOTREACHED()
      << "Should never be reached but is necessary for some compilers.";
  return std::string();
}

}  // namespace views
