// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SUPERVISED_SUPERVISED_ICON_STRING_H_
#define ASH_SYSTEM_SUPERVISED_SUPERVISED_ICON_STRING_H_

#include "base/strings/string16.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

const gfx::VectorIcon& GetSupervisedUserIcon();

base::string16 GetSupervisedUserMessage();

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_INFO_VIEW_H_
