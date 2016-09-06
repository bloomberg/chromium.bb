// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_ELEVATION_HELPERS_H
#define REMOTING_HOST_WIN_ELEVATION_HELPERS_H

namespace remoting {

// Determines whether the current process is elevated.
bool IsProcessElevated();

// Determines whether the current process has the UiAccess privilege.
bool CurrentProcessHasUiAccess();

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_ELEVATION_HELPERS_H