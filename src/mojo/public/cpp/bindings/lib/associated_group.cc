// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_group.h"

#include "mojo/public/cpp/bindings/associated_group_controller.h"

namespace mojo {

AssociatedGroup::AssociatedGroup() = default;

AssociatedGroup::AssociatedGroup(
    scoped_refptr<AssociatedGroupController> controller)
    : controller_(std::move(controller)) {}

AssociatedGroup::AssociatedGroup(const ScopedInterfaceEndpointHandle& handle)
    : controller_getter_(handle.CreateGroupControllerGetter()) {}

AssociatedGroup::AssociatedGroup(const AssociatedGroup& other) = default;

AssociatedGroup::~AssociatedGroup() = default;

AssociatedGroup& AssociatedGroup::operator=(const AssociatedGroup& other) =
    default;

AssociatedGroupController* AssociatedGroup::GetController() {
  if (controller_)
    return controller_.get();

  return controller_getter_.Run();
}

}  // namespace mojo
