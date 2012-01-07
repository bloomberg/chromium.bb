// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/resource_array_dev.h"

#include "ppapi/c/dev/ppb_resource_array_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_ResourceArray_Dev>() {
  return PPB_RESOURCEARRAY_DEV_INTERFACE;
}

}  // namespace

ResourceArray_Dev::ResourceArray_Dev() {
}

ResourceArray_Dev::ResourceArray_Dev(PassRef, PP_Resource resource) {
  PassRefFromConstructor(resource);
}

ResourceArray_Dev::ResourceArray_Dev(const ResourceArray_Dev& other)
    : Resource(other) {
}

ResourceArray_Dev::ResourceArray_Dev(Instance* instance,
                                     const PP_Resource elements[],
                                     uint32_t size) {
  if (has_interface<PPB_ResourceArray_Dev>() && instance) {
    PassRefFromConstructor(get_interface<PPB_ResourceArray_Dev>()->Create(
        instance->pp_instance(), elements, size));
  }
}

ResourceArray_Dev::~ResourceArray_Dev() {
}

ResourceArray_Dev& ResourceArray_Dev::operator=(
    const ResourceArray_Dev& other) {
  Resource::operator=(other);
  return *this;
}

uint32_t ResourceArray_Dev::size() const {
  if (!has_interface<PPB_ResourceArray_Dev>())
    return 0;
  return get_interface<PPB_ResourceArray_Dev>()->GetSize(pp_resource());
}

PP_Resource ResourceArray_Dev::operator[](uint32_t index) const {
  if (!has_interface<PPB_ResourceArray_Dev>())
    return 0;
  return get_interface<PPB_ResourceArray_Dev>()->GetAt(pp_resource(), index);
}

}  // namespace pp
