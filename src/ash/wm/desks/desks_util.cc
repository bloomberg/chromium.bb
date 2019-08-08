// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_util.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"

namespace ash {

namespace desks_util {

namespace {

constexpr std::array<int, kMaxNumberOfDesks> kDesksContainersIds = {
    // TODO(afakhry): Fill this.
    kShellWindowId_DefaultContainerDeprecated,
};

}  // namespace

const std::array<int, kMaxNumberOfDesks>& GetDesksContainersIds() {
  return kDesksContainersIds;
}

const char* GetDeskContainerName(int container_id) {
  switch (container_id) {
    case kShellWindowId_DefaultContainerDeprecated:
      return "Desk_Container_A";

      // TODO(afakhry): Fill this.

    default:
      NOTREACHED();
      return "";
  }
}

std::vector<aura::Window*> GetDesksContainers(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  std::vector<aura::Window*> containers;
  for (const auto& id : kDesksContainersIds) {
    auto* container = root->GetChildById(id);
    DCHECK(container);
    containers.emplace_back(container);
  }

  return containers;
}

bool IsDeskContainer(const aura::Window* container) {
  DCHECK(container);
  // TODO(afakhry): Add the rest of the desks containers.
  return container->id() == kShellWindowId_DefaultContainerDeprecated;
}

bool IsDeskContainerId(int id) {
  // TODO(afakhry): Add the rest of the desks containers.
  return id == kShellWindowId_DefaultContainerDeprecated;
}

int GetActiveDeskContainerId() {
  // TODO(afakhry): Do proper checking when the other desks containers are
  // added.
  return kShellWindowId_DefaultContainerDeprecated;
}

ASH_EXPORT bool IsActiveDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return container->id() == GetActiveDeskContainerId();
}

aura::Window* GetActiveDeskContainerForRoot(aura::Window* root) {
  DCHECK(root);
  return root->GetChildById(GetActiveDeskContainerId());
}

}  // namespace desks_util

}  // namespace ash
