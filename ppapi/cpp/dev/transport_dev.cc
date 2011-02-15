// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/transport_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Transport_Dev>() {
  return PPB_TRANSPORT_DEV_INTERFACE;
}

}  // namespace

Transport_Dev::Transport_Dev(Instance* instance,
                             const char* name,
                             const char* proto) {
  if (has_interface<PPB_Transport_Dev>())
    PassRefFromConstructor(get_interface<PPB_Transport_Dev>()->CreateTransport(
        instance->pp_instance(), name, proto));
}

}  // namespace pp

