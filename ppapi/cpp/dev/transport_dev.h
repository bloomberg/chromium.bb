// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_TRANSPORT_DEV_H_
#define PPAPI_CPP_DEV_TRANSPORT_DEV_H_

#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

class Transport_Dev : public Resource {
 public:
  Transport_Dev() {}
  Transport_Dev(Instance* instance, const char* name, const char* proto);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_TRANSPORT_DEV_H_

