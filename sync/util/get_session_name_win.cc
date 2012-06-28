// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/get_session_name_win.h"

#include <windows.h>

namespace syncer {
namespace internal {

std::string GetComputerName() {
  char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD size = sizeof(computer_name);
  if (GetComputerNameA(computer_name, &size))
    return computer_name;
  return std::string();
}

}  // namespace internal
}  // namespace syncer
