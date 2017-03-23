// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_unique_id.h"

namespace ui {

namespace {

using UniqueIdMap = base::hash_map<int32_t, AXPlatformNode*>;
// Map from each AXPlatformNode's unique id to its instance.
base::LazyInstance<UniqueIdMap>::DestructorAtExit g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;
}

#if !defined(OS_WIN)
// This is the default implementation for platforms where native views
// accessibility is unsupported or unfinished.
//
// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return nullptr;
}
#endif

AXPlatformNode::AXPlatformNode() : unique_id_(GetNextAXPlatformNodeUniqueId()) {
  g_unique_id_map.Get()[unique_id_] = this;
}

AXPlatformNode::~AXPlatformNode() {
  if (unique_id_)
    g_unique_id_map.Get().erase(unique_id_);
}

void AXPlatformNode::Destroy() {
  g_unique_id_map.Get().erase(unique_id_);
  unique_id_ = 0;
}

AXPlatformNode* AXPlatformNode::GetFromUniqueId(int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end())
    return iter->second;

  return nullptr;
}

}  // namespace ui
