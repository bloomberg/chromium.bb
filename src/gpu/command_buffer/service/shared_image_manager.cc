// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_manager.h"

namespace gpu {

SharedImageManager::SharedImageManager() = default;

SharedImageManager::~SharedImageManager() {
  DCHECK(images_.empty());
}

bool SharedImageManager::Register(const Mailbox& mailbox,
                                  std::unique_ptr<SharedImageBacking> backing) {
  auto found = images_.find(mailbox);
  if (found != images_.end())
    return false;

  images_.emplace(mailbox,
                  BackingAndRefCount(std::move(backing), 1 /* ref_count */));
  return true;
}

void SharedImageManager::Unregister(const Mailbox& mailbox, bool have_context) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::Unregister: Trying to unregister a "
                  "not-registered mailbox.";
    return;
  }

  found->second.ref_count--;
  if (found->second.ref_count == 0) {
    found->second.backing->Destroy(have_context);
    images_.erase(found);
  }
}

SharedImageManager::BackingAndRefCount::BackingAndRefCount(
    std::unique_ptr<SharedImageBacking> backing,
    uint32_t ref_count)
    : backing(std::move(backing)), ref_count(ref_count) {}
SharedImageManager::BackingAndRefCount::BackingAndRefCount(
    BackingAndRefCount&& other) = default;
SharedImageManager::BackingAndRefCount& SharedImageManager::BackingAndRefCount::
operator=(BackingAndRefCount&& rhs) = default;
SharedImageManager::BackingAndRefCount::~BackingAndRefCount() = default;

}  // namespace gpu
