// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/win/load_string_from_resource.h"

namespace remoting {

CAtlString LoadStringFromResource(int id) {
  CAtlString s;
  if (!s.LoadString(id)) {
    s.Format(L"Missing resource %d", id);
  }
  return s;
}

}  // namespace remoting
