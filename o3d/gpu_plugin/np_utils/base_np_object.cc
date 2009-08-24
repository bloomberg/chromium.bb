// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/base_np_object.h"

namespace o3d {
namespace gpu_plugin {

BaseNPObject::BaseNPObject(NPP npp) : npp_(npp) {
}

BaseNPObject::~BaseNPObject() {
}

// The default implementations of the virtual functions return failure and clear
// the result variant to void if appropriate.

void BaseNPObject::Invalidate() {
}

bool BaseNPObject::HasMethod(NPIdentifier name) {
  return false;
}

bool BaseNPObject::Invoke(NPIdentifier name,
                          const NPVariant* args,
                          uint32_t num_args,
                          NPVariant* result) {
  VOID_TO_NPVARIANT(*result);
  return false;
}

bool BaseNPObject::InvokeDefault(const NPVariant* args,
                                 uint32_t num_args,
                                 NPVariant* result) {
  VOID_TO_NPVARIANT(*result);
  return false;
}

bool BaseNPObject::HasProperty(NPIdentifier name) {
  return false;
}

bool BaseNPObject::GetProperty(NPIdentifier name, NPVariant* result) {
  VOID_TO_NPVARIANT(*result);
  return false;
}

bool BaseNPObject::SetProperty(NPIdentifier name, const NPVariant* value) {
  return false;
}

bool BaseNPObject::RemoveProperty(NPIdentifier name) {
  return false;
}

bool BaseNPObject::Enumerate(NPIdentifier** names, uint32_t* count) {
  return false;
}

bool BaseNPObject::Construct(const NPVariant* args,
                             uint32_t num_args,
                             NPVariant* result) {
  VOID_TO_NPVARIANT(*result);
  return false;
}

// These implementations of the NPClass functions forward to the virtual
// functions in BaseNPObject.

void BaseNPObject::DeallocateImpl(NPObject* object) {
  delete static_cast<BaseNPObject*>(object);
}

void BaseNPObject::InvalidateImpl(NPObject* object) {
  return static_cast<BaseNPObject*>(object)->Invalidate();
}

bool BaseNPObject::HasMethodImpl(NPObject* object, NPIdentifier name) {
  return static_cast<BaseNPObject*>(object)->HasMethod(name);
}

bool BaseNPObject::InvokeImpl(NPObject* object,
                              NPIdentifier name,
                              const NPVariant* args,
                              uint32_t num_args,
                              NPVariant* result) {
  return static_cast<BaseNPObject*>(object)->Invoke(
      name, args, num_args, result);
}

bool BaseNPObject::InvokeDefaultImpl(NPObject* object,
                                     const NPVariant* args,
                                     uint32_t num_args,
                                     NPVariant* result) {
  return static_cast<BaseNPObject*>(object)->InvokeDefault(
      args, num_args, result);
}

bool BaseNPObject::HasPropertyImpl(NPObject* object, NPIdentifier name) {
  return static_cast<BaseNPObject*>(object)->HasProperty(name);
}

bool BaseNPObject::GetPropertyImpl(NPObject* object,
                                   NPIdentifier name,
                                   NPVariant* result) {
  return static_cast<BaseNPObject*>(object)->GetProperty(name, result);
}

bool BaseNPObject::SetPropertyImpl(NPObject* object,
                                   NPIdentifier name,
                                   const NPVariant* value) {
  return static_cast<BaseNPObject*>(object)->SetProperty(name, value);
}

bool BaseNPObject::RemovePropertyImpl(NPObject* object, NPIdentifier name) {
  return static_cast<BaseNPObject*>(object)->RemoveProperty(name);
}

bool BaseNPObject::EnumerateImpl(NPObject* object,
                                 NPIdentifier** names,
                                 uint32_t* count) {
  return static_cast<BaseNPObject*>(object)->Enumerate(names, count);
}

bool BaseNPObject::ConstructImpl(NPObject* object,
                                 const NPVariant* args,
                                 uint32_t num_args,
                                 NPVariant* result) {
  return static_cast<BaseNPObject*>(object)->Construct(
      args, num_args, result);
}

}  // namespace gpu_plugin
}  // namespace o3d
