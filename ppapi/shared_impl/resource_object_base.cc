// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource_object_base.h"

namespace ppapi {

ResourceObjectBase::~ResourceObjectBase() {
}

#define DEFINE_TYPE_GETTER(RESOURCE) \
  thunk::RESOURCE* ResourceObjectBase::As##RESOURCE() { return NULL; }
FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_TYPE_GETTER)
#undef DEFINE_TYPE_GETTER

}  // namespace ppapi

