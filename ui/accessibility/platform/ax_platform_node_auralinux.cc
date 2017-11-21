// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect_conversions.h"

// Some ATK interfaces require returning a (const gchar*), use
// this macro to make it safe to return a pointer to a temporary
// string.
#define RETURN_STRING(str_expr) \
  {                             \
    static std::string result;  \
    result = (str_expr);        \
    return result.c_str();      \
  }

//
// ax_platform_node_auralinux AtkObject definition and implementation.
//

G_BEGIN_DECLS

#define AX_PLATFORM_NODE_AURALINUX_TYPE (ax_platform_node_auralinux_get_type())
#define AX_PLATFORM_NODE_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST( \
        (obj), AX_PLATFORM_NODE_AURALINUX_TYPE, AXPlatformNodeAuraLinuxObject))
#define AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST( \
        (klass), AX_PLATFORM_NODE_AURALINUX_TYPE, AXPlatformNodeAuraLinuxClass))
#define IS_AX_PLATFORM_NODE_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define IS_AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define AX_PLATFORM_NODE_AURALINUX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS( \
        (obj), AX_PLATFORM_NODE_AURALINUX_TYPE, AXPlatformNodeAuraLinuxClass))

typedef struct _AXPlatformNodeAuraLinuxObject AXPlatformNodeAuraLinuxObject;
typedef struct _AXPlatformNodeAuraLinuxClass AXPlatformNodeAuraLinuxClass;

struct _AXPlatformNodeAuraLinuxObject {
  AtkObject parent;
  ui::AXPlatformNodeAuraLinux* m_object;
};

struct _AXPlatformNodeAuraLinuxClass {
  AtkObjectClass parent_class;
};

GType ax_platform_node_auralinux_get_type();

static gpointer ax_platform_node_auralinux_parent_class = nullptr;

static ui::AXPlatformNodeAuraLinux* ToAXPlatformNodeAuraLinux(
    AXPlatformNodeAuraLinuxObject* atk_object) {
  if (!atk_object)
    return nullptr;

  return atk_object->m_object;
}

static ui::AXPlatformNodeAuraLinux* AtkObjectToAXPlatformNodeAuraLinux(
    AtkObject* atk_object) {
  if (!atk_object)
    return nullptr;

  if (IS_AX_PLATFORM_NODE_AURALINUX(atk_object))
    return ToAXPlatformNodeAuraLinux(AX_PLATFORM_NODE_AURALINUX(atk_object));

  return nullptr;
}

static const gchar* ax_platform_node_auralinux_get_name(AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (obj->GetStringAttribute(ui::AX_ATTR_NAME).empty() &&
      !(obj->GetIntAttribute(ui::AX_ATTR_NAME_FROM) ==
        ui::AX_NAME_FROM_ATTRIBUTE_EXPLICITLY_EMPTY))
    return nullptr;

  return obj->GetStringAttribute(ui::AX_ATTR_NAME).c_str();
}

static const gchar* ax_platform_node_auralinux_get_description(
    AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(
      ui::AX_ATTR_DESCRIPTION).c_str();
}

static gint ax_platform_node_auralinux_get_index_in_parent(
    AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
    AtkObjectToAXPlatformNodeAuraLinux(atk_object);

  if (!obj || !obj->GetParent())
    return -1;

  AtkObject* obj_parent = obj->GetParent();

  unsigned child_count = atk_object_get_n_accessible_children(obj_parent);
  for (unsigned index = 0; index < child_count; index++) {
    AtkObject* child = atk_object_ref_accessible_child(obj_parent, index);
    bool atk_object_found = child == atk_object;
    g_object_unref(child);
    if (atk_object_found)
      return index;
  }

  return obj->GetIndexInParent();
}

static AtkObject* ax_platform_node_auralinux_get_parent(AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetParent();
}

static gint ax_platform_node_auralinux_get_n_children(AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->GetChildCount();
}

static AtkObject* ax_platform_node_auralinux_ref_child(
    AtkObject* atk_object, gint index) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->ChildAtIndex(index);
  if (result)
    g_object_ref(result);
  return result;
}

static AtkRelationSet* ax_platform_node_auralinux_ref_relation_set(
    AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  AtkRelationSet* atk_relation_set =
      ATK_OBJECT_CLASS(ax_platform_node_auralinux_parent_class)->
      ref_relation_set(atk_object);

  if (!obj)
    return atk_relation_set;

  obj->GetAtkRelations(atk_relation_set);
  return atk_relation_set;
}

static AtkAttributeSet* ax_platform_node_auralinux_get_attributes(
    AtkObject* atk_object) {
  return NULL;
}

static AtkRole ax_platform_node_auralinux_get_role(AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->GetAtkRole();
}

static AtkStateSet* ax_platform_node_auralinux_ref_state_set(
    AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return NULL;

  AtkStateSet* atk_state_set =
      ATK_OBJECT_CLASS(ax_platform_node_auralinux_parent_class)->
      ref_state_set(atk_object);

  obj->GetAtkState(atk_state_set);
  return atk_state_set;
}

//
// AtkComponent interface
//

static gfx::Point FindAtkObjectParentCoords(AtkObject* atk_object) {
  if (!atk_object)
    return gfx::Point(0, 0);

  if (atk_object_get_role(atk_object) == ATK_ROLE_WINDOW) {
    int x, y;
    atk_component_get_extents(ATK_COMPONENT(atk_object),
        &x, &y, nullptr, nullptr, ATK_XY_WINDOW);
    gfx::Point window_coords(x, y);
    return window_coords;
  }
  atk_object = atk_object_get_parent(atk_object);

  return FindAtkObjectParentCoords(atk_object);
}

static void ax_platform_node_auralinux_get_extents(AtkComponent* atk_component,
                                                   gint* x, gint* y,
                                                   gint* width, gint* height,
                                                   AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;
  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetExtents(x, y, width, height, coord_type);
}

static void ax_platform_node_auralinux_get_position(AtkComponent* atk_component,
                                                    gint* x, gint* y,
                                                    AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

static void ax_platform_node_auralinux_get_size(AtkComponent* atk_component,
                                                gint* width, gint* height) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

static AtkObject* ax_platform_node_auralinux_ref_accessible_at_point(
    AtkComponent* atk_component,
    gint x,
    gint y,
    AtkCoordType coord_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), nullptr);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->HitTestSync(x, y, coord_type);
}

static gboolean ax_platform_node_auralinux_grab_focus(
    AtkComponent* atk_component) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->GrabFocus();
}

void ax_component_interface_base_init(AtkComponentIface* iface) {
  iface->get_extents = ax_platform_node_auralinux_get_extents;
  iface->get_position = ax_platform_node_auralinux_get_position;
  iface->get_size = ax_platform_node_auralinux_get_size;
  iface->ref_accessible_at_point =
      ax_platform_node_auralinux_ref_accessible_at_point;
  iface->grab_focus = ax_platform_node_auralinux_grab_focus;
}

static const GInterfaceInfo ComponentInfo = {
  reinterpret_cast<GInterfaceInitFunc>(ax_component_interface_base_init), 0, 0
};

//
// AtkAction interface
//

static gboolean ax_platform_node_auralinux_do_action(AtkAction* atk_action,
                                                     gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), FALSE);
  g_return_val_if_fail(!index, FALSE);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->DoDefaultAction();
}

static gint ax_platform_node_auralinux_get_n_actions(AtkAction* atk_action) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return 1;
}

static const gchar* ax_platform_node_auralinux_get_action_description(
    AtkAction*,
    gint) {
  // Not implemented. Right now Orca does not provide this and
  // Chromium is not providing a string for the action description.
  return nullptr;
}

static const gchar* ax_platform_node_auralinux_get_action_name(
    AtkAction* atk_action,
    gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDefaultActionName();
}

static const gchar* ax_platform_node_auralinux_get_action_keybinding(
    AtkAction* atk_action,
    gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ui::AX_ATTR_ACCESS_KEY).c_str();
}

void ax_action_interface_base_init(AtkActionIface* iface) {
  iface->do_action = ax_platform_node_auralinux_do_action;
  iface->get_n_actions = ax_platform_node_auralinux_get_n_actions;
  iface->get_description = ax_platform_node_auralinux_get_action_description;
  iface->get_name = ax_platform_node_auralinux_get_action_name;
  iface->get_keybinding = ax_platform_node_auralinux_get_action_keybinding;
}

static const GInterfaceInfo ActionInfo = {
    reinterpret_cast<GInterfaceInitFunc>(ax_action_interface_base_init),
    nullptr, nullptr};

//
// The rest of the AXPlatformNodeAtk code, not specific to one
// of the Atk* interfaces.
//

static void ax_platform_node_auralinux_init(AtkObject* atk_object,
                                            gpointer data) {
  if (ATK_OBJECT_CLASS(ax_platform_node_auralinux_parent_class)->initialize) {
    ATK_OBJECT_CLASS(ax_platform_node_auralinux_parent_class)->initialize(
        atk_object, data);
  }

  AX_PLATFORM_NODE_AURALINUX(atk_object)->m_object =
      reinterpret_cast<ui::AXPlatformNodeAuraLinux*>(data);
}

static void ax_platform_node_auralinux_finalize(GObject* atk_object) {
  G_OBJECT_CLASS(ax_platform_node_auralinux_parent_class)->finalize(atk_object);
}

static void ax_platform_node_auralinux_class_init(AtkObjectClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  ax_platform_node_auralinux_parent_class = g_type_class_peek_parent(klass);

  gobject_class->finalize = ax_platform_node_auralinux_finalize;
  klass->initialize = ax_platform_node_auralinux_init;
  klass->get_name = ax_platform_node_auralinux_get_name;
  klass->get_description = ax_platform_node_auralinux_get_description;
  klass->get_parent = ax_platform_node_auralinux_get_parent;
  klass->get_n_children = ax_platform_node_auralinux_get_n_children;
  klass->ref_child = ax_platform_node_auralinux_ref_child;
  klass->get_role = ax_platform_node_auralinux_get_role;
  klass->ref_state_set = ax_platform_node_auralinux_ref_state_set;
  klass->get_index_in_parent = ax_platform_node_auralinux_get_index_in_parent;
  klass->ref_relation_set = ax_platform_node_auralinux_ref_relation_set;
  klass->get_attributes = ax_platform_node_auralinux_get_attributes;
}

GType ax_platform_node_auralinux_get_type() {
  static volatile gsize type_volatile = 0;

#if !GLIB_CHECK_VERSION(2, 36, 0)
  g_type_init();
#endif

  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo tinfo = {
        sizeof(AXPlatformNodeAuraLinuxClass),
        (GBaseInitFunc) nullptr,
        (GBaseFinalizeFunc) nullptr,
        (GClassInitFunc)ax_platform_node_auralinux_class_init,
        (GClassFinalizeFunc) nullptr,
        nullptr,                               /* class data */
        sizeof(AXPlatformNodeAuraLinuxObject), /* instance size */
        0,                                     /* nb preallocs */
        (GInstanceInitFunc) nullptr,
        nullptr /* value table */
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "AXPlatformNodeAuraLinux", &tinfo, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

void ax_platform_node_auralinux_detach(
    AXPlatformNodeAuraLinuxObject* atk_object) {
  atk_object->m_object = nullptr;
}

G_END_DECLS

namespace ui {

const char* AXPlatformNodeAuraLinux::GetUniqueAccessibilityGTypeName(
    int interface_mask) {
  // 37 characters is enough for "AXPlatformNodeAuraLinux%x" with any integer
  // value.
  static char name[37];
  snprintf(name, sizeof(name), "AXPlatformNodeAuraLinux%x", interface_mask);
  return name;
}

int AXPlatformNodeAuraLinux::GetGTypeInterfaceMask() {
  int interface_mask = 0;

  // Component interface is always supported.
  interface_mask |= 1 << ATK_COMPONENT_INTERFACE;

  // Action interface is basic one. It just relays on executing default action
  // for each object.
  interface_mask |= 1 << ATK_ACTION_INTERFACE;

  // Value Interface
  int role = GetAtkRole();
  if (role == ui::AX_ROLE_PROGRESS_INDICATOR ||
      role == ui::AX_ROLE_SCROLL_BAR || role == ui::AX_ROLE_SLIDER) {
    interface_mask |= 1 << ATK_VALUE_INTERFACE;
  }

  // Document Interface
  if (role == ui::AX_ROLE_DOCUMENT || role == ui::AX_ROLE_ROOT_WEB_AREA ||
      role == ui::AX_ROLE_WEB_AREA)
    interface_mask |= 1 << ATK_DOCUMENT_INTERFACE;

  // Image Interface
  if (role == ui::AX_ROLE_IMAGE || role == ui::AX_ROLE_IMAGE_MAP)
    interface_mask |= 1 << ATK_IMAGE_INTERFACE;

  return interface_mask;
}

GType AXPlatformNodeAuraLinux::GetAccessibilityGType() {
  static const GTypeInfo type_info = {
      sizeof(AXPlatformNodeAuraLinuxClass),
      (GBaseInitFunc) nullptr,
      (GBaseFinalizeFunc) nullptr,
      (GClassInitFunc) nullptr,
      (GClassFinalizeFunc) nullptr,
      nullptr,                               /* class data */
      sizeof(AXPlatformNodeAuraLinuxObject), /* instance size */
      0,                                     /* nb preallocs */
      (GInstanceInitFunc) nullptr,
      nullptr /* value table */
  };

  int interface_mask = GetGTypeInterfaceMask();
  const char* atk_type_name = GetUniqueAccessibilityGTypeName(interface_mask);
  GType type = g_type_from_name(atk_type_name);
  if (type)
    return type;

  type = g_type_register_static(AX_PLATFORM_NODE_AURALINUX_TYPE, atk_type_name,
                                &type_info, GTypeFlags(0));
  if (interface_mask & (1 << ATK_COMPONENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &ComponentInfo);
  if (interface_mask & (1 << ATK_ACTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_ACTION, &ActionInfo);

  return type;
}

AtkObject* AXPlatformNodeAuraLinux::CreateAtkObject() {
#if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (first_time) {
    g_type_init();
    first_time = false;
  }
#endif
  GType type = GetAccessibilityGType();
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, nullptr));

  atk_object_initialize(atk_object, this);

  return ATK_OBJECT(atk_object);
}

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeAuraLinux* node = new AXPlatformNodeAuraLinux();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return AtkObjectToAXPlatformNodeAuraLinux(accessible);
}

//
// AXPlatformNodeAuraLinux implementation.
//

// static
AXPlatformNode* AXPlatformNodeAuraLinux::application_ = nullptr;

// static
void AXPlatformNodeAuraLinux::SetApplication(AXPlatformNode* application) {
  application_ = application;
}

// static
void AXPlatformNodeAuraLinux::StaticInitialize() {
  AtkUtilAuraLinux::GetInstance()->InitializeAsync();
}

AtkRole AXPlatformNodeAuraLinux::GetAtkRole() {
  switch (GetData().role) {
    case ui::AX_ROLE_ALERT:
      return ATK_ROLE_ALERT;
    case ui::AX_ROLE_ALERT_DIALOG:
      return ATK_ROLE_ALERT;
    case ui::AX_ROLE_APPLICATION:
      return ATK_ROLE_APPLICATION;
    case ui::AX_ROLE_AUDIO:
#if defined(ATK_CHECK_VERSION)
#if ATK_CHECK_VERSION(2, 12, 0)
      return ATK_ROLE_AUDIO;
#else
      return ATK_ROLE_SECTION;
#endif
#else
      return ATK_ROLE_SECTION;
#endif
    case ui::AX_ROLE_BUTTON:
      return ATK_ROLE_PUSH_BUTTON;
    case ui::AX_ROLE_CANVAS:
      return ATK_ROLE_CANVAS;
    case ui::AX_ROLE_CAPTION:
      return ATK_ROLE_CAPTION;
    case ui::AX_ROLE_CHECK_BOX:
      return ATK_ROLE_CHECK_BOX;
    case ui::AX_ROLE_COLOR_WELL:
      return ATK_ROLE_COLOR_CHOOSER;
    case ui::AX_ROLE_COLUMN_HEADER:
      return ATK_ROLE_COLUMN_HEADER;
    case ui::AX_ROLE_COMBO_BOX_GROUPING:
      return ATK_ROLE_COMBO_BOX;
    case ui::AX_ROLE_COMBO_BOX_MENU_BUTTON:
      return ATK_ROLE_COMBO_BOX;
    case ui::AX_ROLE_DATE:
      return ATK_ROLE_DATE_EDITOR;
    case ui::AX_ROLE_DATE_TIME:
      return ATK_ROLE_DATE_EDITOR;
    case ui::AX_ROLE_DIALOG:
      return ATK_ROLE_DIALOG;
    case ui::AX_ROLE_DOCUMENT:
      return ATK_ROLE_DOCUMENT_WEB;
    case ui::AX_ROLE_FORM:
      return ATK_ROLE_FORM;
    case ui::AX_ROLE_GENERIC_CONTAINER:
      return ATK_ROLE_PANEL;
    case ui::AX_ROLE_GROUP:
      return ATK_ROLE_PANEL;
    case ui::AX_ROLE_IGNORED:
      return ATK_ROLE_REDUNDANT_OBJECT;
    case ui::AX_ROLE_IMAGE:
      return ATK_ROLE_IMAGE;
    case ui::AX_ROLE_IMAGE_MAP:
      return ATK_ROLE_IMAGE_MAP;
    case ui::AX_ROLE_LABEL_TEXT:
      return ATK_ROLE_LABEL;
    case ui::AX_ROLE_LINK:
      return ATK_ROLE_LINK;
    case ui::AX_ROLE_LIST:
      return ATK_ROLE_LIST;
    case ui::AX_ROLE_LIST_BOX:
      return ATK_ROLE_LIST_BOX;
    case ui::AX_ROLE_LIST_ITEM:
      return ATK_ROLE_LIST_ITEM;
    case ui::AX_ROLE_MATH:
#if defined(ATK_CHECK_VERSION)
#if ATK_CHECK_VERSION(2, 12, 0)
      return ATK_ROLE_MATH;
#else
      return ATK_ROLE_TEXT;
#endif
#else
      return ATK_ROLE_TEXT;
#endif
    case ui::AX_ROLE_MENU:
      return ATK_ROLE_MENU;
    case ui::AX_ROLE_MENU_BAR:
      return ATK_ROLE_MENU_BAR;
    case ui::AX_ROLE_MENU_ITEM:
      return ATK_ROLE_MENU_ITEM;
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
      return ATK_ROLE_CHECK_MENU_ITEM;
    case ui::AX_ROLE_MENU_ITEM_RADIO:
      return ATK_ROLE_RADIO_MENU_ITEM;
    case ui::AX_ROLE_METER:
      return ATK_ROLE_PROGRESS_BAR;
    case ui::AX_ROLE_PARAGRAPH:
      return ATK_ROLE_PARAGRAPH;
    case ui::AX_ROLE_RADIO_BUTTON:
      return ATK_ROLE_RADIO_BUTTON;
    case ui::AX_ROLE_ROW_HEADER:
      return ATK_ROLE_ROW_HEADER;
    case ui::AX_ROLE_ROOT_WEB_AREA:
      return ATK_ROLE_DOCUMENT_WEB;
    case ui::AX_ROLE_SCROLL_BAR:
      return ATK_ROLE_SCROLL_BAR;
    case ui::AX_ROLE_SLIDER:
      return ATK_ROLE_SLIDER;
    case ui::AX_ROLE_SPIN_BUTTON:
      return ATK_ROLE_SPIN_BUTTON;
    case ui::AX_ROLE_SPLITTER:
      return ATK_ROLE_SEPARATOR;
    case ui::AX_ROLE_STATIC_TEXT:
      return ATK_ROLE_TEXT;
    case ui::AX_ROLE_STATUS:
      return ATK_ROLE_STATUSBAR;
    case ui::AX_ROLE_TAB:
      return ATK_ROLE_PAGE_TAB;
    case ui::AX_ROLE_TABLE:
      return ATK_ROLE_TABLE;
    case ui::AX_ROLE_TAB_LIST:
      return ATK_ROLE_PAGE_TAB_LIST;
    case ui::AX_ROLE_TEXT_FIELD:
      return ATK_ROLE_ENTRY;
    case ui::AX_ROLE_TEXT_FIELD_WITH_COMBO_BOX:
      return ATK_ROLE_COMBO_BOX;
    case ui::AX_ROLE_TOGGLE_BUTTON:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ui::AX_ROLE_TOOLBAR:
      return ATK_ROLE_TOOL_BAR;
    case ui::AX_ROLE_TOOLTIP:
      return ATK_ROLE_TOOL_TIP;
    case ui::AX_ROLE_TREE:
      return ATK_ROLE_TREE;
    case ui::AX_ROLE_TREE_ITEM:
      return ATK_ROLE_TREE_ITEM;
    case ui::AX_ROLE_TREE_GRID:
      return ATK_ROLE_TREE_TABLE;
    case ui::AX_ROLE_VIDEO:
#if defined(ATK_CHECK_VERSION)
#if ATK_CHECK_VERSION(2, 12, 0)
      return ATK_ROLE_VIDEO;
#else
      return ATK_ROLE_SECTION;
#endif
#else
      return ATK_ROLE_SECTION;
#endif
    case ui::AX_ROLE_WEB_AREA:
      return ATK_ROLE_DOCUMENT_WEB;
    case ui::AX_ROLE_WINDOW:
      return ATK_ROLE_WINDOW;
    default:
      return ATK_ROLE_UNKNOWN;
  }
}

void AXPlatformNodeAuraLinux::GetAtkState(AtkStateSet* atk_state_set) {
  AXNodeData data = GetData();
  if (data.HasState(ui::AX_STATE_DEFAULT))
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFAULT);
  if (data.HasState(ui::AX_STATE_EDITABLE))
    atk_state_set_add_state(atk_state_set, ATK_STATE_EDITABLE);
  if (data.HasState(ui::AX_STATE_EXPANDED))
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDED);
  if (data.HasState(ui::AX_STATE_FOCUSABLE))
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSABLE);
#if defined(ATK_CHECK_VERSION)
#if ATK_CHECK_VERSION(2, 11, 2)
  if (data.HasState(ui::AX_STATE_HASPOPUP))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HAS_POPUP);
#endif
#endif
  if (data.HasState(ui::AX_STATE_SELECTED))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTED);
  if (data.HasState(ui::AX_STATE_SELECTABLE))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE);

  // Checked state
  const auto checked_state = static_cast<ui::AXCheckedState>(
      GetIntAttribute(ui::AX_ATTR_CHECKED_STATE));
  switch (checked_state) {
    case ui::AX_CHECKED_STATE_MIXED:
      atk_state_set_add_state(atk_state_set, ATK_STATE_INDETERMINATE);
      break;
    case ui::AX_CHECKED_STATE_TRUE:
      atk_state_set_add_state(atk_state_set,
                              data.role == ui::AX_ROLE_TOGGLE_BUTTON
                                  ? ATK_STATE_PRESSED
                                  : ATK_STATE_CHECKED);
      break;
    default:
      break;
  }

  switch (GetIntAttribute(ui::AX_ATTR_RESTRICTION)) {
    case ui::AX_RESTRICTION_NONE:
      atk_state_set_add_state(atk_state_set, ATK_STATE_ENABLED);
      break;
    case ui::AX_RESTRICTION_READ_ONLY:
#if defined(ATK_CHECK_VERSION)
#if ATK_CHECK_VERSION(2, 16, 0)
      atk_state_set_add_state(atk_state_set, ATK_STATE_READ_ONLY);
#endif
#endif
      break;
  }

  if (delegate_->GetFocus() == GetNativeViewAccessible())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSED);
}

void AXPlatformNodeAuraLinux::GetAtkRelations(AtkRelationSet* atk_relation_set)
{
}

AXPlatformNodeAuraLinux::AXPlatformNodeAuraLinux()
    : atk_object_(nullptr) {
}

AXPlatformNodeAuraLinux::~AXPlatformNodeAuraLinux() {
  g_object_unref(atk_object_);
}

void AXPlatformNodeAuraLinux::Init(AXPlatformNodeDelegate* delegate) {
  // Initialize ATK.
  AXPlatformNodeBase::Init(delegate);
  atk_object_ = CreateAtkObject();
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetNativeViewAccessible() {
  return atk_object_;
}

void AXPlatformNodeAuraLinux::NotifyAccessibilityEvent(ui::AXEvent event_type) {
}

int AXPlatformNodeAuraLinux::GetIndexInParent() {
  return 0;
}

void AXPlatformNodeAuraLinux::SetExtentsRelativeToAtkCoordinateType(
    gint* x, gint* y, gint* width, gint* height, AtkCoordType coord_type) {
  gfx::Rect extents = GetBoundsInScreen();

  if (x)
    *x = extents.x();
  if (y)
    *y = extents.y();
  if (width)
    *width = extents.width();
  if (height)
    *height = extents.height();

  if (coord_type == ATK_XY_WINDOW) {
    if (AtkObject* atk_object = GetParent()) {
      gfx::Point window_coords = FindAtkObjectParentCoords(atk_object);
      if (x)
        *x -= window_coords.x();
      if (y)
        *y -= window_coords.y();
    }
  }
}

void AXPlatformNodeAuraLinux::GetExtents(gint* x, gint* y,
                                         gint* width, gint* height,
                                         AtkCoordType coord_type) {
  SetExtentsRelativeToAtkCoordinateType(x, y,
                                        width, height,
                                        coord_type);
}

void AXPlatformNodeAuraLinux::GetPosition(gint* x, gint* y,
                                          AtkCoordType coord_type) {
  SetExtentsRelativeToAtkCoordinateType(x, y,
                                        nullptr, nullptr,
                                        coord_type);
}

void AXPlatformNodeAuraLinux::GetSize(gint* width, gint* height) {
  gfx::Rect rect_size = gfx::ToEnclosingRect(GetData().location);
  if (width)
    *width = rect_size.width();
  if (height)
    *height = rect_size.height();
}

gfx::NativeViewAccessible
AXPlatformNodeAuraLinux::HitTestSync(gint x, gint y, AtkCoordType coord_type) {
  if (coord_type == ATK_XY_WINDOW) {
    if (AtkObject* atk_object = GetParent()) {
      gfx::Point window_coords = FindAtkObjectParentCoords(atk_object);
      x += window_coords.x();
      y += window_coords.y();
    }
  }

  return delegate_->HitTestSync(x, y);
}

bool AXPlatformNodeAuraLinux::GrabFocus() {
  AXActionData action_data;
  action_data.action = AX_ACTION_FOCUS;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::DoDefaultAction() {
  AXActionData action_data;
  action_data.action = AX_ACTION_DO_DEFAULT;
  return delegate_->AccessibilityPerformAction(action_data);
}

const gchar* AXPlatformNodeAuraLinux::GetDefaultActionName() {
  int action;
  if (!GetIntAttribute(ui::AX_ATTR_DEFAULT_ACTION_VERB, &action))
    return nullptr;

  base::string16 action_verb = ui::ActionVerbToUnlocalizedString(
      static_cast<ui::AXDefaultActionVerb>(action));

  RETURN_STRING(base::UTF16ToUTF8(action_verb));
}

}  // namespace ui
