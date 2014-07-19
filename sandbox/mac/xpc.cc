// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/xpc.h"

namespace sandbox {

bool InitializeXPC() {
  std::vector<std::string> path_list;
  path_list.push_back("/usr/lib/system/libxpc.dylib");

  sandbox_mac::StubPathMap path_map;
  path_map[sandbox_mac::kModuleXpc_stubs] = path_list;
  path_map[sandbox_mac::kModuleXpc_private_stubs] = path_list;

  return sandbox_mac::InitializeStubs(path_map);
}

}  // namespace sandbox
