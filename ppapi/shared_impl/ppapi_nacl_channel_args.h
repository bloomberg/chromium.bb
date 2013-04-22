// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_CREATE_NACL_CHANNEL_ARGS_H
#define PPAPI_SHARED_IMPL_PPAPI_CREATE_NACL_CHANNEL_ARGS_H

#include <string>
#include <vector>

#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT PpapiNaClChannelArgs {
 public:
  PpapiNaClChannelArgs();
  ~PpapiNaClChannelArgs();

  bool off_the_record;
  PpapiPermissions permissions;

  // Switches from the command-line.
  std::vector<std::string> switch_names;
  std::vector<std::string> switch_values;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_CREATE_NACL_CHANNEL_ARGS_H
