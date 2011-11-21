// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_SHADOW_TYPES_H_
#define UI_AURA_CLIENT_SHADOW_TYPES_H_
#pragma once

namespace aura {

// Different types of drop shadows that can be drawn under a window by the
// shell.  Used as a value for the kShadowTypeKey property.
enum AURA_EXPORT ShadowType {
  SHADOW_TYPE_NONE = 0,
  SHADOW_TYPE_RECTANGULAR,
};

}  // namespace aura

#endif  // UI_AURA_CLIENT_SHADOW_TYPES_H_
