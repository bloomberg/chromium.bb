// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_
#define UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_

#include <atk/atk.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace ui {

// Implements accessibility on Aura Linux using ATK.
class AXPlatformNodeAuraLinux : public AXPlatformNodeBase {
 public:
  AXPlatformNodeAuraLinux();

  // Set or get the root-level Application object that's the parent of all
  // top-level windows.
  AX_EXPORT static void SetApplication(AXPlatformNode* application);
  static AXPlatformNode* application() { return application_; }

  // Do asynchronous static initialization.
  AX_EXPORT static void StaticInitialize();

  AtkRole GetAtkRole();
  void GetAtkState(AtkStateSet* state_set);
  void GetAtkRelations(AtkRelationSet* atk_relation_set);
  void GetExtents(gint* x, gint* y, gint* width, gint* height,
                  AtkCoordType coord_type);
  void GetPosition(gint* x, gint* y, AtkCoordType coord_type);
  void GetSize(gint* width, gint* height);
  gfx::NativeViewAccessible HitTestSync(gint x,
                                        gint y,
                                        AtkCoordType coord_type);
  bool GrabFocus();
  bool DoDefaultAction();
  const gchar* GetDefaultActionName();

  void SetExtentsRelativeToAtkCoordinateType(
      gint* x, gint* y, gint* width, gint* height,
      AtkCoordType coord_type);

  // AXPlatformNode overrides.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;

  // AXPlatformNodeBase overrides.
  void Init(AXPlatformNodeDelegate* delegate) override;
  int GetIndexInParent() override;

 private:
  ~AXPlatformNodeAuraLinux() override;

  enum AtkInterfaces {
    ATK_ACTION_INTERFACE,
    ATK_COMPONENT_INTERFACE,
    ATK_DOCUMENT_INTERFACE,
    ATK_EDITABLE_TEXT_INTERFACE,
    ATK_HYPERLINK_INTERFACE,
    ATK_HYPERTEXT_INTERFACE,
    ATK_IMAGE_INTERFACE,
    ATK_SELECTION_INTERFACE,
    ATK_TABLE_INTERFACE,
    ATK_TEXT_INTERFACE,
    ATK_VALUE_INTERFACE,
  };
  static const char* GetUniqueAccessibilityGTypeName(int interface_mask);
  int GetGTypeInterfaceMask();
  GType GetAccessibilityGType();
  AtkObject* CreateAtkObject();

  // We own a reference to this ref-counted object.
  AtkObject* atk_object_;

  // The root-level Application object that's the parent of all
  // top-level windows.
  static AXPlatformNode* application_;

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeAuraLinux);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_
