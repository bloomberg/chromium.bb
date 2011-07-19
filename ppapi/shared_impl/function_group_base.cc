// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/function_group_base.h"

namespace ppapi {

FunctionGroupBase::~FunctionGroupBase() {
}

#define DEFINE_TYPE_GETTER(FUNCTIONS) \
  thunk::FUNCTIONS* FunctionGroupBase::As##FUNCTIONS() { return NULL; }
FOR_ALL_PPAPI_FUNCTION_APIS(DEFINE_TYPE_GETTER)
#undef DEFINE_TYPE_GETTER

}  // namespace ppapi
