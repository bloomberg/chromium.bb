// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_GET_SESSION_NAME_WIN_H_
#define SYNC_UTIL_GET_SESSION_NAME_WIN_H_
#pragma once

#include <string>

namespace csync {
namespace internal {

std::string GetComputerName();

}  // namespace internal
}  // namespace csync

#endif  // SYNC_UTIL_GET_SESSION_NAME_WIN_H_
