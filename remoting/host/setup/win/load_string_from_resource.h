// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_WIN_LOAD_STRING_FROM_RESOURCE_H_
#define REMOTING_HOST_SETUP_WIN_LOAD_STRING_FROM_RESOURCE_H_

#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>

namespace remoting {

// Loads a string from a resource.
CAtlString LoadStringFromResource(int id);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_WIN_LOAD_STRING_FROM_RESOURCE_H_
