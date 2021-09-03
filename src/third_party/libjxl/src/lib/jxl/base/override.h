// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_BASE_OVERRIDE_H_
#define LIB_JXL_BASE_OVERRIDE_H_

// 'Trool' for command line arguments: force enable/disable, or use default.

namespace jxl {

// No effect if kDefault, otherwise forces a feature (typically a FrameHeader
// flag) on or off.
enum class Override : int { kOn = 1, kOff = 0, kDefault = -1 };

static inline Override OverrideFromBool(bool flag) {
  return flag ? Override::kOn : Override::kOff;
}

static inline bool ApplyOverride(Override o, bool default_condition) {
  if (o == Override::kOn) return true;
  if (o == Override::kOff) return false;
  return default_condition;
}

}  // namespace jxl

#endif  // LIB_JXL_BASE_OVERRIDE_H_
