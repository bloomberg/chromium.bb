// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TRIBOOL_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TRIBOOL_H_

#include <string>

namespace signin {

// The values are persisted to disk and must not be changed.
enum class Tribool { kUnknown = -1, kFalse = 0, kTrue = 1 };

// Returns the string representation of a tribool.
std::string TriboolToString(Tribool tribool);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TRIBOOL_H_
