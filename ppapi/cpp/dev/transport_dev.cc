// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/transport_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_Transport_Dev> transport_f(PPB_TRANSPORT_DEV_INTERFACE);

}  // namespace

namespace pp {

Transport_Dev::Transport_Dev(const char* name,
                             const char* proto) {
  if (transport_f)
    PassRefFromConstructor(
        transport_f->CreateTransport(Module::Get()->pp_module(), name, proto));
}

}  // namespace pp

