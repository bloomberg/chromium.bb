// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_TEST_UTIL_H_
#define REMOTING_HOST_SETUP_TEST_UTIL_H_

#include "base/platform_file.h"

namespace remoting {

// Creates an anonymous, unidirectional pipe, returning true if successful. On
// success, the caller is responsible for closing both ends using
// base::ClosePlatformFile().
bool MakePipe(base::PlatformFile* read_handle,
              base::PlatformFile* write_handle);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_TEST_UTIL_H_
