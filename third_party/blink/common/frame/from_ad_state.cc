// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/from_ad_state.h"

namespace blink {

FromAdState GetFromAdState(bool is_ad_subframe, bool is_ad_script_in_stack) {
  return is_ad_subframe
             ? is_ad_script_in_stack ? FromAdState::kAdScriptAndAdFrame
                                     : FromAdState::kNonAdScriptAndAdFrame
             : is_ad_script_in_stack ? FromAdState::kAdScriptAndNonAdFrame
                                     : FromAdState::kNonAdScriptAndNonAdFrame;
}

}  // namespace blink
