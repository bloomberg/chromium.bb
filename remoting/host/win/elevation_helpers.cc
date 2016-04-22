// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/elevation_helpers.h"

#include <windows.h>

#include "base/win/scoped_handle.h"

namespace remoting {

bool IsProcessElevated() {
  HANDLE process_token;
  OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token);

  base::win::ScopedHandle scoped_process_token(process_token);

  // Unlike TOKEN_ELEVATION_TYPE which returns TokenElevationTypeDefault when
  // UAC is turned off, TOKEN_ELEVATION will tell you the process is elevated.
  DWORD size;
  TOKEN_ELEVATION elevation;
  GetTokenInformation(process_token, TokenElevation,
                      &elevation, sizeof(elevation), &size);
  return elevation.TokenIsElevated != 0;
}

}  // namespace remoting