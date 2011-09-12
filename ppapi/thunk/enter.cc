// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"

#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

EnterResourceCreation::EnterResourceCreation(PP_Instance instance)
    : EnterFunctionNoLock<ResourceCreationAPI>(instance, true) {
}

EnterResourceCreation::~EnterResourceCreation() {
}

EnterInstance::EnterInstance(PP_Instance instance)
    : EnterFunctionNoLock<PPB_Instance_FunctionAPI>(instance, true) {
}

EnterInstance::~EnterInstance() {
}

}  // namespace thunk
}  // namespace ppapi
