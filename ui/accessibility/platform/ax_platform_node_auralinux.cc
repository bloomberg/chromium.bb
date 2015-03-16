// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

#include "base/command_line.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

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

static AtkRole ax_platform_node_auralinux_get_role(AtkObject* atk_object) {
  ui::AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->GetAtkRole();
}

//
// The rest of the AXPlatformNodeAuraLinux code, not specific to one
// of the Atk* interfaces.
//

static gpointer ax_platform_node_auralinux_parent_class = nullptr;

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
}

GType ax_platform_node_auralinux_get_type() {
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo tinfo = {
      sizeof(AXPlatformNodeAuraLinuxClass),
      (GBaseInitFunc) 0,
      (GBaseFinalizeFunc) 0,
      (GClassInitFunc) ax_platform_node_auralinux_class_init,
      (GClassFinalizeFunc) 0,
      0, /* class data */
      sizeof(AXPlatformNodeAuraLinuxObject), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) 0,
      0 /* value table */
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "AXPlatformNodeAuraLinux", &tinfo, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

AXPlatformNodeAuraLinuxObject* ax_platform_node_auralinux_new(
    ui::AXPlatformNodeAuraLinux* obj) {
  #if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (first_time) {
    g_type_init();
    first_time = false;
  }
  #endif

  GType type = AX_PLATFORM_NODE_AURALINUX_TYPE;
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, 0));
  atk_object_initialize(atk_object, obj);
  return AX_PLATFORM_NODE_AURALINUX(atk_object);
}

void ax_platform_node_auralinux_detach(
    AXPlatformNodeAuraLinuxObject* atk_object) {
  atk_object->m_object = nullptr;
}

G_END_DECLS

//
// AXPlatformNodeAuraLinux implementation.
//

namespace ui {

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeAuraLinux* node = new AXPlatformNodeAuraLinux();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNodeAuraLinux::application_ = nullptr;

// static
void AXPlatformNodeAuraLinux::SetApplication(AXPlatformNode* application) {
  application_ = application;
  AtkUtilAuraLinux::GetInstance();
}

AtkRole AXPlatformNodeAuraLinux::GetAtkRole() {
  switch (GetData().role) {
    case ui::AX_ROLE_APPLICATION:
      return ATK_ROLE_APPLICATION;
    case ui::AX_ROLE_BUTTON:
      return ATK_ROLE_PUSH_BUTTON;
    case ui::AX_ROLE_CHECK_BOX:
      return ATK_ROLE_CHECK_BOX;
    case ui::AX_ROLE_COMBO_BOX:
      return ATK_ROLE_COMBO_BOX;
    case ui::AX_ROLE_STATIC_TEXT:
      return ATK_ROLE_TEXT;
    case ui::AX_ROLE_TEXT_FIELD:
      return ATK_ROLE_ENTRY;
    case ui::AX_ROLE_WINDOW:
      return ATK_ROLE_WINDOW;
    default:
      return ATK_ROLE_UNKNOWN;
  }
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
  atk_object_ = ATK_OBJECT(ax_platform_node_auralinux_new(this));
}

void AXPlatformNodeAuraLinux::Destroy() {
  delegate_ = nullptr;
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetNativeViewAccessible() {
  return atk_object_;
}

void AXPlatformNodeAuraLinux::NotifyAccessibilityEvent(ui::AXEvent event_type) {
}

int AXPlatformNodeAuraLinux::GetIndexInParent() {
  return 0;
}

}  // namespace ui
