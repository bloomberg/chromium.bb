// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/constants.h"

#include "base/stringize_macros.h"

namespace remoting {

#if defined(OS_WIN)

// The Omaha Appid of the host. It should be kept in sync with $(var.OmahaAppid)
// defined in remoting/host/installer/chromoting.wxs and the Omaha server
// configuration.
const char16 kHostOmahaAppid[] =
    TO_L_STRING("{b210701e-ffc4-49e3-932b-370728c72662}");

#endif  // defined(OS_WIN)

}  // namespace remoting
