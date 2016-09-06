// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/elevation_helpers.h"

#include <windows.h>

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace {

void GetCurrentProcessToken(base::win::ScopedHandle* scoped_handle) {
  DCHECK(scoped_handle);

  HANDLE process_token;
  OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token);
  scoped_handle->Set(process_token);
  DCHECK(scoped_handle->IsValid());
}

}  // namespace

namespace remoting {

bool IsProcessElevated() {
  base::win::ScopedHandle scoped_process_token;
  GetCurrentProcessToken(&scoped_process_token);

  // Unlike TOKEN_ELEVATION_TYPE which returns TokenElevationTypeDefault when
  // UAC is turned off, TOKEN_ELEVATION returns whether the process is elevated.
  DWORD size;
  TOKEN_ELEVATION elevation;
  if (!GetTokenInformation(scoped_process_token.Get(), TokenElevation,
                           &elevation, sizeof(elevation), &size)) {
    PLOG(ERROR) << "GetTokenInformation() failed";
    return false;
  }
  return elevation.TokenIsElevated != 0;
}

bool CurrentProcessHasUiAccess() {
  base::win::ScopedHandle scoped_process_token;
  GetCurrentProcessToken(&scoped_process_token);

  DWORD size;
  DWORD uiaccess_value = 0;
  if (!GetTokenInformation(scoped_process_token.Get(), TokenUIAccess,
                           &uiaccess_value, sizeof(uiaccess_value), &size)) {
    PLOG(ERROR) << "GetTokenInformation() failed";
    return false;
  }
  return uiaccess_value != 0;
}

}  // namespace remoting