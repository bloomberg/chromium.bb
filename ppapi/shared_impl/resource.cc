// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource.h"

#include "base/logging.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracker_base.h"

namespace ppapi {

Resource::Resource(PP_Instance instance) {
  // The instance should always be valid (nonzero).
  DCHECK(instance);

  // For the in-process case, the host resource and resource are the same.
  //
  // AddResource needs our instance() getter to work, and that goes through
  // the host resource, so we need to fill that first even though we don't
  // have a resource ID yet, then fill the resource in later.
  host_resource_ = HostResource::MakeInstanceOnly(instance);
  pp_resource_ = TrackerBase::Get()->GetResourceTracker()->AddResource(this);
  host_resource_.SetHostResource(instance, pp_resource_);
}

Resource::Resource(const HostResource& host_resource)
    : host_resource_(host_resource) {
  pp_resource_ = TrackerBase::Get()->GetResourceTracker()->AddResource(this);
}

Resource::~Resource() {
  TrackerBase::Get()->GetResourceTracker()->RemoveResource(this);
}

PP_Resource Resource::GetReference() {
  TrackerBase::Get()->GetResourceTracker()->AddRefResource(pp_resource());
  return pp_resource();
}

void Resource::LastPluginRefWasDeleted() {
}

void Resource::InstanceWasDeleted() {
  host_resource_ = HostResource();
}

#define DEFINE_TYPE_GETTER(RESOURCE) \
  thunk::RESOURCE* Resource::As##RESOURCE() { return NULL; }
FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_TYPE_GETTER)
#undef DEFINE_TYPE_GETTER

}  // namespace ppapi

