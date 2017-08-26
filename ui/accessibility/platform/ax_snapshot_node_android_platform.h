// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_SNAPSHOT_ANDROID_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_SNAPSHOT_ANDROID_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class AXNode;
class AXTree;

// An intermediate representation of the accessibility snapshot
// in order to share code between ARC and Android. The field names
// are kept consistent with AccessibilitySnapshotNode.java.
struct AXSnapshotNodeAndroid {
  // Builds a whole tree of AXSnapshotNodeAndroid objects from
  // an AXTreeUpdate.
  AX_EXPORT static std::unique_ptr<AXSnapshotNodeAndroid> Create(
      const AXTreeUpdate& update,
      bool show_password);

  // Returns a fake Android view class that is a closest
  // approximation of the AXRole.
  AX_EXPORT static const char* AXRoleToAndroidClassName(AXRole role,
                                                        bool has_parent);

  AX_EXPORT static bool AXRoleIsLink(AXRole role);

  AX_EXPORT static base::string16 AXUrlBaseText(base::string16 url);

  AX_EXPORT ~AXSnapshotNodeAndroid();

  gfx::Rect rect;
  base::string16 text;
  float text_size;
  int32_t color;
  int32_t bgcolor;
  bool bold;
  bool italic;
  bool underline;
  bool line_through;

  bool has_selection;
  int32_t start_selection;
  int32_t end_selection;

  base::Optional<std::string> role;
  std::string class_name;
  std::vector<std::unique_ptr<AXSnapshotNodeAndroid>> children;

 private:
  AXSnapshotNodeAndroid();

  struct WalkAXTreeConfig {
    bool should_select_leaf;
    const bool show_password;
  };

  static std::unique_ptr<AXSnapshotNodeAndroid> WalkAXTreeDepthFirst(
      const AXNode* node,
      gfx::Rect rect,
      const AXTreeUpdate& update,
      const AXTree* tree,
      WalkAXTreeConfig& config);

  DISALLOW_COPY_AND_ASSIGN(AXSnapshotNodeAndroid);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_SNAPSHOT_ANDROID_H_
