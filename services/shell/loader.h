// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_LOADER_H_
#define SERVICES_SHELL_LOADER_H_

#include "base/callback.h"
#include "mojo/public/cpp/system/core.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace shell {

// Interface to implement special loading behavior for a particular name.
class Loader {
 public:
  virtual ~Loader() {}

  virtual void Load(const std::string& name,
                    mojom::ShellClientRequest request) = 0;
};

}  // namespace shell

#endif  // SERVICES_SHELL_LOADER_H_
