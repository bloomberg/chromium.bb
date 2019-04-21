// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

#include <dlfcn.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_atk_hyperlink.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 12, 0)
#define ATK_212
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 16, 0)
#define ATK_216
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 26, 0)
#define ATK_226
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 30, 0)
#define ATK_230
#endif

namespace ui {

namespace {

// We must forward declare this because it is used by the traditional GObject
// type manipulation macros.
namespace atk_object {
GType GetType();
}  // namespace atk_object

//
// ax_platform_node_auralinux AtkObject definition and implementation.
//
#define AX_PLATFORM_NODE_AURALINUX_TYPE (atk_object::GetType())
#define AX_PLATFORM_NODE_AURALINUX(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                              AXPlatformNodeAuraLinuxObject))
#define AX_PLATFORM_NODE_AURALINUX_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                           AXPlatformNodeAuraLinuxClass))
#define IS_AX_PLATFORM_NODE_AURALINUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define IS_AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define AX_PLATFORM_NODE_AURALINUX_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                             AXPlatformNodeAuraLinuxClass))

typedef struct _AXPlatformNodeAuraLinuxObject AXPlatformNodeAuraLinuxObject;
typedef struct _AXPlatformNodeAuraLinuxClass AXPlatformNodeAuraLinuxClass;

struct _AXPlatformNodeAuraLinuxObject {
  AtkObject parent;
  AXPlatformNodeAuraLinux* m_object;
};

struct _AXPlatformNodeAuraLinuxClass {
  AtkObjectClass parent_class;
};

// The root-level Application object that's the parent of all top-level windows.
AXPlatformNode* g_root_application = nullptr;

// The last AtkObject with keyboard focus. Tracking this is required to emit the
// ATK_STATE_FOCUSED change to false.
AtkObject* g_current_focused = nullptr;

// The last object which was selected. Tracking this is required because
// widgets in the browser UI only emit notifications upon becoming selected,
// but clients also expect notifications when items become unselected.
AXPlatformNodeAuraLinux* g_current_selected = nullptr;

// The AtkObject with role=ATK_ROLE_FRAME that represents the toplevel desktop
// window with focus. If this window is not one of our windows, this value
// should be null. This is a weak pointer as well, so its value will also be
// null if if the AtkObject is destroyed.
AtkObject* g_active_top_level_frame = nullptr;

// AtkTableCell was introduced in ATK 2.12. Ubuntu Trusty has ATK 2.10.
// Compile-time checks are in place for ATK versions that are older than 2.12.
// However, we also need runtime checks in case the version we are building
// against is newer than the runtime version. To prevent a runtime error, we
// check that we have a version of ATK that supports AtkTableCell. If we do,
// we dynamically load the symbol; if we don't, the interface is absent from
// the accessible object and its methods will not be exposed or callable.
// The definitions below ensure we have no missing symbols. Note that in
// environments where we have ATK > 2.12, the definitions of AtkTableCell and
// AtkTableCellIface below are overridden by the runtime version.
// TODO(accessibility) Remove these definitions, along with the use of
// LoadTableCellMethods() when 2.12 is the minimum supported version.
typedef struct _AtkTableCell AtkTableCell;
typedef struct _AtkTableCellIface AtkTableCellIface;
typedef GType (*cell_get_type_func)();
typedef GPtrArray* (*get_column_header_cells_func)(AtkTableCell* cell);
typedef GPtrArray* (*get_row_header_cells_func)(AtkTableCell* cell);
typedef bool (*get_row_column_span_func)(AtkTableCell* cell,
                                         gint* row,
                                         gint* column,
                                         gint* row_span,
                                         gint* col_span);

cell_get_type_func cell_get_type = nullptr;
get_column_header_cells_func get_column_header_cells = nullptr;
get_row_header_cells_func get_row_header_cells = nullptr;
get_row_column_span_func get_row_column_span = nullptr;

bool LoadTableCellMethods() {
  cell_get_type = reinterpret_cast<cell_get_type_func>(
      dlsym(RTLD_DEFAULT, "atk_table_cell_get_type"));
  get_column_header_cells = reinterpret_cast<get_column_header_cells_func>(
      dlsym(RTLD_DEFAULT, "atk_table_cell_get_column_header_cells"));
  get_row_header_cells = reinterpret_cast<get_row_header_cells_func>(
      dlsym(RTLD_DEFAULT, "atk_table_cell_get_row_header_cells"));
  get_row_column_span = reinterpret_cast<get_row_column_span_func>(
      dlsym(RTLD_DEFAULT, "atk_table_cell_get_row_column_span"));

  return cell_get_type;
}

AtkObject* FindAtkObjectParentFrame(AtkObject* atk_object) {
  while (atk_object) {
    if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME)
      return atk_object;
    atk_object = atk_object_get_parent(atk_object);
  }
  return nullptr;
}

bool IsFrameAncestorOfAtkObject(AtkObject* frame, AtkObject* atk_object) {
  AtkObject* current_frame = FindAtkObjectParentFrame(atk_object);
  while (current_frame) {
    if (current_frame == frame)
      return true;
    current_frame =
        FindAtkObjectParentFrame(atk_object_get_parent(current_frame));
  }
  return false;
}

// Returns a stack of AtkObjects of activated popup menus. Since each popup
// menu and submenu has its own native window, we want to properly manage the
// activated state for their containing frames.
std::vector<AtkObject*>& GetActiveMenus() {
  static base::NoDestructor<std::vector<AtkObject*>> active_menus;
  return *active_menus;
}

// The currently active frame is g_active_top_level_frame, unless there is an
// active menu. If there is an active menu the parent frame of the
// most-recently opened active menu should be the currently active frame.
AtkObject* ComputeActiveTopLevelFrame() {
  if (!GetActiveMenus().empty())
    return FindAtkObjectParentFrame(GetActiveMenus().back());
  return g_active_top_level_frame;
}

const char* GetUniqueAccessibilityGTypeName(int interface_mask) {
  // 37 characters is enough for "AXPlatformNodeAuraLinux%x" with any integer
  // value.
  static char name[37];
  snprintf(name, sizeof(name), "AXPlatformNodeAuraLinux%x", interface_mask);
  return name;
}

void SetWeakGPtrToAtkObject(AtkObject** weak_pointer, AtkObject* new_value) {
  if (*weak_pointer == new_value)
    return;

  if (*weak_pointer) {
    g_object_remove_weak_pointer(G_OBJECT(*weak_pointer),
                                 reinterpret_cast<void**>(weak_pointer));
  }

  *weak_pointer = new_value;

  if (new_value) {
    g_object_add_weak_pointer(G_OBJECT(new_value),
                              reinterpret_cast<void**>(weak_pointer));
  }
}

void SetActiveTopLevelFrame(AtkObject* new_top_level_frame) {
  SetWeakGPtrToAtkObject(&g_active_top_level_frame, new_top_level_frame);
}

AXCoordinateSystem AtkCoordTypeToAXCoordinateSystem(
    AtkCoordType coordinate_type) {
  switch (coordinate_type) {
    case ATK_XY_SCREEN:
      return AXCoordinateSystem::kScreen;
    case ATK_XY_WINDOW:
      return AXCoordinateSystem::kRootFrame;
#ifdef ATK_230
    case ATK_XY_PARENT:
      // AXCoordinateSystem does not support parent coordinates.
      NOTIMPLEMENTED();
      return AXCoordinateSystem::kFrame;
#endif  // ATK_230
    default:
      return AXCoordinateSystem::kScreen;
  }
}

void IdsToGPtrArray(AXPlatformNodeDelegate* delegate,
                    const std::vector<int32_t>& ids,
                    GPtrArray* array) {
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = delegate->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        g_ptr_array_add(array, atk_header);
      }
    }
  }
}

const char* BuildDescriptionFromHeaders(AXPlatformNodeDelegate* delegate,
                                        const std::vector<int32_t>& ids) {
  std::vector<std::string> names;
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = delegate->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible())
        names.push_back(atk_object_get_name(atk_header));
    }
  }

  std::string result = base::JoinString(names, " ");
  return g_strdup(result.c_str());
}

gfx::Point FindAtkObjectParentCoords(AtkObject* atk_object) {
  if (!atk_object)
    return gfx::Point(0, 0);

  if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME) {
    int x, y;
    atk_component_get_extents(ATK_COMPONENT(atk_object), &x, &y, nullptr,
                              nullptr, ATK_XY_WINDOW);
    gfx::Point window_coords(x, y);
    return window_coords;
  }
  atk_object = atk_object_get_parent(atk_object);

  return FindAtkObjectParentCoords(atk_object);
}

AtkAttributeSet* PrependAtkAttributeToAtkAttributeSet(
    const char* name,
    const char* value,
    AtkAttributeSet* attribute_set) {
  AtkAttribute* attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  attribute->name = g_strdup(name);
  attribute->value = g_strdup(value);
  return g_slist_prepend(attribute_set, attribute);
}

AXPlatformNodeAuraLinux* ToAXPlatformNodeAuraLinux(
    AXPlatformNodeAuraLinuxObject* atk_object) {
  if (!atk_object)
    return nullptr;

  return atk_object->m_object;
}

AXPlatformNodeAuraLinux* AtkObjectToAXPlatformNodeAuraLinux(
    AtkObject* atk_object) {
  if (!atk_object)
    return nullptr;

  if (IS_AX_PLATFORM_NODE_AURALINUX(atk_object))
    return ToAXPlatformNodeAuraLinux(AX_PLATFORM_NODE_AURALINUX(atk_object));

  return nullptr;
}

// TODO(aleventhal) Remove this and use atk_role_get_name() once the following
// GNOME bug is fixed: https://bugzilla.gnome.org/show_bug.cgi?id=795983
const char* const kRoleNames[] = {
    "invalid",  // ATK_ROLE_INVALID.
    "accelerator label",
    "alert",
    "animation",
    "arrow",
    "calendar",
    "canvas",
    "check box",
    "check menu item",
    "color chooser",
    "column header",
    "combo box",
    "dateeditor",
    "desktop icon",
    "desktop frame",
    "dial",
    "dialog",
    "directory pane",
    "drawing area",
    "file chooser",
    "filler",
    "fontchooser",
    "frame",
    "glass pane",
    "html container",
    "icon",
    "image",
    "internal frame",
    "label",
    "layered pane",
    "list",
    "list item",
    "menu",
    "menu bar",
    "menu item",
    "option pane",
    "page tab",
    "page tab list",
    "panel",
    "password text",
    "popup menu",
    "progress bar",
    "push button",
    "radio button",
    "radio menu item",
    "root pane",
    "row header",
    "scroll bar",
    "scroll pane",
    "separator",
    "slider",
    "split pane",
    "spin button",
    "statusbar",
    "table",
    "table cell",
    "table column header",
    "table row header",
    "tear off menu item",
    "terminal",
    "text",
    "toggle button",
    "tool bar",
    "tool tip",
    "tree",
    "tree table",
    "unknown",
    "viewport",
    "window",
    "header",
    "footer",
    "paragraph",
    "ruler",
    "application",
    "autocomplete",
    "edit bar",
    "embedded component",
    "entry",
    "chart",
    "caption",
    "document frame",
    "heading",
    "page",
    "section",
    "redundant object",
    "form",
    "link",
    "input method window",
    "table row",
    "tree item",
    "document spreadsheet",
    "document presentation",
    "document text",
    "document web",
    "document email",
    "comment",
    "list box",
    "grouping",
    "image map",
    "notification",
    "info bar",
    "level bar",
    "title bar",
    "block quote",
    "audio",
    "video",
    "definition",
    "article",
    "landmark",
    "log",
    "marquee",
    "math",
    "rating",
    "timer",
    "description list",
    "description term",
    "description value",
    "static",
    "math fraction",
    "math root",
    "subscript",
    "superscript",
    "footnote",  // ATK_ROLE_FOOTNOTE = 122.
};

#if defined(ATK_216)
constexpr AtkRole kStaticRole = ATK_ROLE_STATIC;
constexpr AtkRole kSubscriptRole = ATK_ROLE_SUBSCRIPT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_SUPERSCRIPT;
#else
constexpr AtkRole kStaticRole = ATK_ROLE_TEXT;
constexpr AtkRole kSubscriptRole = ATK_ROLE_TEXT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_TEXT;
#endif

#if defined(ATK_226)
constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_FOOTNOTE;
#else
constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_LIST_ITEM;
#endif

namespace atk_component {

void GetExtents(AtkComponent* atk_component,
                gint* x,
                gint* y,
                gint* width,
                gint* height,
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
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetExtents(x, y, width, height, coord_type);
}

void GetPosition(AtkComponent* atk_component,
                 gint* x,
                 gint* y,
                 AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

void GetSize(AtkComponent* atk_component, gint* width, gint* height) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

AtkObject* RefAccesibleAtPoint(AtkComponent* atk_component,
                               gint x,
                               gint y,
                               AtkCoordType coord_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), nullptr);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->HitTestSync(x, y, coord_type);
  if (result)
    g_object_ref(result);
  return result;
}

gboolean GrabFocus(AtkComponent* atk_component) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->GrabFocus();
}

void Init(AtkComponentIface* iface) {
  iface->get_extents = GetExtents;
  iface->get_position = GetPosition;
  iface->get_size = GetSize;
  iface->ref_accessible_at_point = RefAccesibleAtPoint;
  iface->grab_focus = GrabFocus;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_component

namespace atk_action {

gboolean DoAction(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), FALSE);
  g_return_val_if_fail(!index, FALSE);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->DoDefaultAction();
}

gint GetNActions(AtkAction* atk_action) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return 1;
}

const gchar* GetDescription(AtkAction*, gint) {
  // Not implemented. Right now Orca does not provide this and
  // Chromium is not providing a string for the action description.
  return nullptr;
}

const gchar* GetName(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDefaultActionName();
}

const gchar* GetKeybinding(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kAccessKey)
      .c_str();
}

void Init(AtkActionIface* iface) {
  iface->do_action = DoAction;
  iface->get_n_actions = GetNActions;
  iface->get_description = GetDescription;
  iface->get_name = GetName;
  iface->get_keybinding = GetKeybinding;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_action

namespace atk_document {

const gchar* GetDocumentAttributeValue(AtkDocument* atk_doc,
                                       const gchar* attribute) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributeValue(attribute);
}

AtkAttributeSet* GetDocumentAttributes(AtkDocument* atk_doc) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributes();
}

void Init(AtkDocumentIface* iface) {
  iface->get_document_attribute_value = GetDocumentAttributeValue;
  iface->get_document_attributes = GetDocumentAttributes;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_document

namespace atk_image {

void GetImagePosition(AtkImage* atk_img,
                      gint* x,
                      gint* y,
                      AtkCoordType coord_type) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

const gchar* GetImageDescription(AtkImage* atk_img) {
  g_return_val_if_fail(ATK_IMAGE(atk_img), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

void GetImageSize(AtkImage* atk_img, gint* width, gint* height) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

void Init(AtkImageIface* iface) {
  iface->get_image_position = GetImagePosition;
  iface->get_image_description = GetImageDescription;
  iface->get_image_size = GetImageSize;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_image

namespace atk_value {

void GetCurrentValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kValueForRange,
                                 value);
}

void GetMinimumValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMinValueForRange,
                                 value);
}

void GetMaximumValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMaxValueForRange,
                                 value);
}

void GetMinimumIncrement(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kStepValueForRange,
                                 value);
}

void Init(AtkValueIface* iface) {
  iface->get_current_value = GetCurrentValue;
  iface->get_maximum_value = GetMaximumValue;
  iface->get_minimum_value = GetMinimumValue;
  iface->get_minimum_increment = GetMinimumIncrement;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_value

namespace atk_hyperlink {

AtkHyperlink* GetHyperlink(AtkHyperlinkImpl* atk_hyperlink_impl) {
  g_return_val_if_fail(ATK_HYPERLINK_IMPL(atk_hyperlink_impl), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_hyperlink_impl);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  AtkHyperlink* atk_hyperlink = obj->GetAtkHyperlink();
  g_object_ref(atk_hyperlink);

  return atk_hyperlink;
}

void Init(AtkHyperlinkImplIface* iface) {
  iface->get_hyperlink = GetHyperlink;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_hyperlink

namespace atk_hypertext {

AtkHyperlink* GetLink(AtkHypertext* hypertext, int index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  if (!obj)
    return nullptr;

  const AXHypertext& ax_hypertext = obj->GetHypertext();
  if (index > static_cast<int>(ax_hypertext.hyperlinks.size()) || index < 0)
    return nullptr;

  int32_t id = ax_hypertext.hyperlinks[index];
  auto* link = static_cast<AXPlatformNodeAuraLinux*>(
      AXPlatformNodeBase::GetFromUniqueId(id));
  if (!link)
    return nullptr;

  AtkHyperlink* atk_hyperlink = link->GetAtkHyperlink();
  for (const auto& key_value : ax_hypertext.hyperlink_offset_to_index) {
    if (key_value.second == index) {
      ax_platform_atk_hyperlink_set_indices(
          AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink), key_value.first,
          key_value.first + 1);
    }
  }

  return atk_hyperlink;
}

int GetNLinks(AtkHypertext* hypertext) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  return obj ? obj->GetHypertext().hyperlinks.size() : 0;
}

int GetLinkIndex(AtkHypertext* hypertext, int char_index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));

  auto it = obj->GetHypertext().hyperlink_offset_to_index.find(char_index);
  if (it == obj->GetHypertext().hyperlink_offset_to_index.end())
    return -1;
  return it->second;
}

void Init(AtkHypertextIface* iface) {
  iface->get_link = GetLink;
  iface->get_n_links = GetNLinks;
  iface->get_link_index = GetLinkIndex;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_hypertext

namespace atk_text {

gchar* GetText(AtkText* atk_text, gint start_offset, gint end_offset) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (start_offset < 0)
    return nullptr;

  base::string16 text = obj->GetText();

  start_offset = obj->UnicodeToUTF16OffsetInText(start_offset);
  if (end_offset < 0) {
    end_offset = text.size();
  } else {
    end_offset = obj->UnicodeToUTF16OffsetInText(end_offset);
    end_offset = std::max(start_offset,
                          std::min(end_offset, static_cast<int>(text.size())));
  }

  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, start_offset);

  return g_strdup(
      base::UTF16ToUTF8(text.substr(start_offset, end_offset - start_offset))
          .c_str());
}

gint GetCharacterCount(AtkText* atk_text) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->UTF16ToUnicodeOffsetInText(obj->GetText().length());
}

AtkAttributeSet* GetRunAttributes(AtkText* atk_text,
                                  gint offset,
                                  gint* start_offset,
                                  gint* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  *start_offset = 0;
  *end_offset = GetCharacterCount(atk_text);

  return nullptr;
}

gunichar GetCharacterAtOffset(AtkText* atk_text, int offset) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  base::string16 text = obj->GetText();
  int32_t text_length = text.length();

  offset = obj->UnicodeToUTF16OffsetInText(offset);
  int32_t limited_offset = std::max(0, std::min(text_length, offset));

  uint32_t code_point;
  base::ReadUnicodeCharacter(text.c_str(), text_length + 1, &limited_offset,
                             &code_point);
  return code_point;
}

// This function returns a single character as a UTF-8 encoded C string because
// the character may be encoded into more than one byte.
char* GetCharacter(AtkText* atk_text,
                   int offset,
                   int* start_offset,
                   int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (offset < 0 || offset >= GetCharacterCount(atk_text))
    return nullptr;

  char* text = GetText(atk_text, offset, offset + 1);
  if (!text)
    return nullptr;

  *start_offset = offset;
  *end_offset = offset + 1;
  return text;
}

char* GetTextWithBoundaryType(AtkText* atk_text,
                              int offset,
                              TextBoundaryType boundary_type,
                              int* start_offset_ptr,
                              int* end_offset_ptr) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  if (offset < 0 || offset >= atk_text_get_character_count(atk_text))
    return nullptr;

  // The offset that we receive from the API is a Unicode character offset.
  // Since we calculate boundaries in terms of UTF-16 code point offsets, we
  // need to convert this input value.
  offset = obj->UnicodeToUTF16OffsetInText(offset);

  int start_offset =
      obj->FindTextBoundary(boundary_type, offset, BACKWARDS_DIRECTION);
  int end_offset =
      obj->FindTextBoundary(boundary_type, offset, FORWARDS_DIRECTION);

  DCHECK_LE(start_offset, end_offset)
      << "Start offset should be less than or equal the end offset.";

  // The ATK API is also expecting Unicode character offsets as output
  // values.
  *start_offset_ptr = obj->UTF16ToUnicodeOffsetInText(start_offset);
  *end_offset_ptr = obj->UTF16ToUnicodeOffsetInText(end_offset);

  base::string16 text = obj->GetText();
  DCHECK_LE(end_offset, static_cast<int>(text.size()));

  base::string16 substr = text.substr(start_offset, end_offset - start_offset);
  return g_strdup(base::UTF16ToUTF8(substr).c_str());
}

base::Optional<TextBoundaryType> AtkTextBoundaryToTextBoundary(
    AtkTextBoundary atk_boundary) {
  // These are listed in order of their definition in the ATK header.
  switch (atk_boundary) {
    case ATK_TEXT_BOUNDARY_CHAR:
      return CHAR_BOUNDARY;
    case ATK_TEXT_BOUNDARY_WORD_START:
      return WORD_BOUNDARY;
    case ATK_TEXT_BOUNDARY_WORD_END:
      return base::nullopt;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      return SENTENCE_BOUNDARY;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      return base::nullopt;
    case ATK_TEXT_BOUNDARY_LINE_START:
      return LINE_BOUNDARY;
    case ATK_TEXT_BOUNDARY_LINE_END:
      return base::nullopt;
  }
  NOTREACHED();
  return base::nullopt;
}

base::Optional<TextBoundaryType> AtkTextGranularityToTextBoundary(
    AtkTextGranularity granularity) {
  // These are listed in order of their definition in the ATK header.
  switch (granularity) {
    case ATK_TEXT_GRANULARITY_CHAR:
      return CHAR_BOUNDARY;
    case ATK_TEXT_GRANULARITY_WORD:
      return WORD_BOUNDARY;
    case ATK_TEXT_GRANULARITY_SENTENCE:
      return SENTENCE_BOUNDARY;
    case ATK_TEXT_GRANULARITY_LINE:
      return LINE_BOUNDARY;
    case ATK_TEXT_GRANULARITY_PARAGRAPH:
      return PARAGRAPH_BOUNDARY;
  }

  return base::nullopt;
}

char* GetTextAtOffset(AtkText* atk_text,
                      int offset,
                      AtkTextBoundary atk_boundary_type,
                      int* start_offset,
                      int* end_offset) {
  if (atk_boundary_type == ATK_TEXT_BOUNDARY_CHAR) {
    return GetCharacter(atk_text, offset, start_offset, end_offset);
  }

  base::Optional<TextBoundaryType> boundary_type =
      AtkTextBoundaryToTextBoundary(atk_boundary_type);
  if (boundary_type.has_value()) {
    return GetTextWithBoundaryType(atk_text, offset, *boundary_type,
                                   start_offset, end_offset);
  }

  *start_offset = -1;
  *end_offset = -1;
  return nullptr;
}

char* GetTextAfterOffset(AtkText* atk_text,
                         int offset,
                         AtkTextBoundary boundary_type,
                         int* start_offset,
                         int* end_offset) {
  if (boundary_type != ATK_TEXT_BOUNDARY_CHAR) {
    *start_offset = -1;
    *end_offset = -1;
    return nullptr;
  }

  return GetCharacter(atk_text, offset + 1, start_offset, end_offset);
}

char* GetTextBeforeOffset(AtkText* atk_text,
                          int offset,
                          AtkTextBoundary boundary_type,
                          int* start_offset,
                          int* end_offset) {
  if (boundary_type != ATK_TEXT_BOUNDARY_CHAR) {
    *start_offset = -1;
    *end_offset = -1;
    return nullptr;
  }

  return GetCharacter(atk_text, offset - 1, start_offset, end_offset);
}

gint GetCaretOffset(AtkText* atk_text) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return -1;
  return obj->GetCaretOffset();
}

gboolean SetCaretOffset(AtkText* atk_text, gint offset) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;
  return obj->SetCaretOffset(offset);
}

int GetNSelections(AtkText* atk_text) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return 0;

  // We only support a single selection.
  return obj->HasSelection() ? 1 : 0;
}

gchar* GetSelection(AtkText* atk_text,
                    int selection_num,
                    int* start_offset,
                    int* end_offset) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return nullptr;
  if (selection_num != 0)
    return nullptr;

  return obj->GetSelection(start_offset, end_offset);
}

gboolean RemoveSelection(AtkText* atk_text, int selection_num) {
  if (selection_num != 0)
    return FALSE;

  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  // Simply collapse the selection to the position of the caret if a caret is
  // visible, otherwise set the selection to 0.
  int selection_end = obj->UTF16ToUnicodeOffsetInText(
      obj->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
  return SetCaretOffset(atk_text, selection_end);
}

gboolean SetSelection(AtkText* atk_text,
                      int selection_num,
                      int start_offset,
                      int end_offset) {
  if (selection_num != 0)
    return FALSE;

  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  return obj->SetTextSelectionForAtkText(start_offset, end_offset);
}

gboolean AddSelection(AtkText* atk_text, int start_offset, int end_offset) {
  // We only support one selection.
  return SetSelection(atk_text, 0, start_offset, end_offset);
}

#if ATK_CHECK_VERSION(2, 10, 0)
char* GetStringAtOffset(AtkText* atk_text,
                        int offset,
                        AtkTextGranularity atk_granularity,
                        int* start_offset,
                        int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  if (atk_granularity == ATK_TEXT_GRANULARITY_CHAR) {
    return GetCharacter(atk_text, offset, start_offset, end_offset);
  }

  base::Optional<TextBoundaryType> granularity =
      AtkTextGranularityToTextBoundary(atk_granularity);
  if (granularity.has_value()) {
    return GetTextWithBoundaryType(atk_text, offset, *granularity, start_offset,
                                   end_offset);
  }

  *start_offset = -1;
  *end_offset = -1;
  return nullptr;
}
#endif

gfx::Rect GetUnclippedParentRangeBoundsRect(
    AXPlatformNodeDelegate* ax_platform_node_delegate,
    const int start_offset,
    const int end_offset) {
  const AXPlatformNode* parent_platform_node =
      AXPlatformNode::FromNativeViewAccessible(
          ax_platform_node_delegate->GetParent());
  if (parent_platform_node) {
    const AXPlatformNodeDelegate* parent_ax_platform_node_delegate =
        parent_platform_node->GetDelegate();
    if (parent_ax_platform_node_delegate) {
      return ax_platform_node_delegate->GetRangeBoundsRect(
                 start_offset, end_offset, AXCoordinateSystem::kRootFrame,
                 AXClippingBehavior::kUnclipped) -
             parent_ax_platform_node_delegate
                 ->GetBoundsRect(AXCoordinateSystem::kRootFrame,
                                 AXClippingBehavior::kClipped)
                 .OffsetFromOrigin();
    }
  }

  return gfx::Rect();
}

void GetCharacterExtents(AtkText* atk_text,
                         int offset,
                         int* x,
                         int* y,
                         int* width,
                         int* height,
                         AtkCoordType coordinate_type) {
  gfx::Rect rect;
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (obj) {
    switch (coordinate_type) {
#ifdef ATK_230
      case ATK_XY_PARENT:
        rect = GetUnclippedParentRangeBoundsRect(obj->GetDelegate(), offset,
                                                 offset + 1);
        break;
#endif
      default:
        rect = obj->GetDelegate()->GetRangeBoundsRect(
            obj->UnicodeToUTF16OffsetInText(offset),
            obj->UnicodeToUTF16OffsetInText(offset + 1),
            AtkCoordTypeToAXCoordinateSystem(coordinate_type),
            AXClippingBehavior::kUnclipped);
        break;
    }
  }

  if (x)
    *x = rect.x();
  if (y)
    *y = rect.y();
  if (width)
    *width = rect.width();
  if (height)
    *height = rect.height();
}

void GetRangeExtents(AtkText* atk_text,
                     int start_offset,
                     int end_offset,
                     AtkCoordType coordinate_type,
                     AtkTextRectangle* out_rectangle) {
  if (!out_rectangle)
    return;

  gfx::Rect rect;
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(atk_text));
  if (obj) {
    switch (coordinate_type) {
#ifdef ATK_230
      case ATK_XY_PARENT:
        rect = GetUnclippedParentRangeBoundsRect(obj->GetDelegate(),
                                                 start_offset, end_offset);
        break;
#endif
      default:
        rect = obj->GetDelegate()->GetRangeBoundsRect(
            obj->UnicodeToUTF16OffsetInText(start_offset),
            obj->UnicodeToUTF16OffsetInText(end_offset),
            AtkCoordTypeToAXCoordinateSystem(coordinate_type),
            AXClippingBehavior::kUnclipped);
        break;
    }
  }

  out_rectangle->x = rect.x();
  out_rectangle->y = rect.y();
  out_rectangle->width = rect.width();
  out_rectangle->height = rect.height();
}

void Init(AtkTextIface* iface) {
  iface->get_text = GetText;
  iface->get_run_attributes = GetRunAttributes;
  iface->get_character_count = GetCharacterCount;
  iface->get_character_at_offset = GetCharacterAtOffset;
  iface->get_text_after_offset = GetTextAfterOffset;
  iface->get_text_before_offset = GetTextBeforeOffset;
  iface->get_text_at_offset = GetTextAtOffset;
  iface->get_caret_offset = GetCaretOffset;
  iface->set_caret_offset = SetCaretOffset;
  iface->get_character_extents = GetCharacterExtents;
  iface->get_range_extents = GetRangeExtents;
  iface->get_n_selections = GetNSelections;
  iface->get_selection = GetSelection;
  iface->add_selection = AddSelection;
  iface->remove_selection = RemoveSelection;
  iface->set_selection = SetSelection;

#if ATK_CHECK_VERSION(2, 10, 0)
  iface->get_string_at_offset = GetStringAtOffset;
#endif
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_text

namespace atk_window {
void Init(AtkWindowIface* iface) {}
const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};
}  // namespace atk_window

namespace atk_selection {

gboolean AddSelection(AtkSelection* selection, gint index) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;
  if (index < 0 || index >= obj->GetChildCount())
    return FALSE;

  AXPlatformNodeAuraLinux* child =
      AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(index));
  DCHECK(child);

  if (!child->SupportsSelectionWithAtkSelection())
    return FALSE;

  bool selected = child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  if (selected)
    return TRUE;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  return child->GetDelegate()->AccessibilityPerformAction(data);
}

gboolean ClearSelection(AtkSelection* selection) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  bool success = true;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    DCHECK(child);

    if (!child->SupportsSelectionWithAtkSelection())
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (!selected)
      continue;

    AXActionData data;
    data.action = ax::mojom::Action::kDoDefault;
    success = success && child->GetDelegate()->AccessibilityPerformAction(data);
  }

  return success;
}

AtkObject* RefSelection(AtkSelection* selection, gint requested_child_index) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return nullptr;

  int child_count = obj->GetChildCount();
  gint selected_count = 0;
  for (int i = 0; i < child_count; ++i) {
    AtkObject* child = obj->ChildAtIndex(i);
    AXPlatformNodeAuraLinux* child_ax_node =
        AtkObjectToAXPlatformNodeAuraLinux(child);
    DCHECK(child_ax_node);

    if (child_ax_node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
      if (selected_count == requested_child_index)
        return static_cast<AtkObject*>(g_object_ref(child));
      ++selected_count;
    }
  }

  return nullptr;
}

gint GetSelectionCount(AtkSelection* selection) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return 0;

  int child_count = obj->GetChildCount();
  gint selected_count = 0;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    DCHECK(child);

    if (child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
      ++selected_count;
  }

  return selected_count;
}

gboolean IsChildSelected(AtkSelection* selection, gint index) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;
  if (index < 0 || index >= obj->GetChildCount())
    return FALSE;

  AXPlatformNodeAuraLinux* child =
      AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(index));
  DCHECK(child);
  return child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

gboolean RemoveSelection(AtkSelection* selection,
                         gint index_into_selected_children) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));

  int child_count = obj->GetChildCount();
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    DCHECK(child);

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (selected && index_into_selected_children == 0) {
      if (!child->SupportsSelectionWithAtkSelection())
        return FALSE;

      AXActionData data;
      data.action = ax::mojom::Action::kDoDefault;
      return child->GetDelegate()->AccessibilityPerformAction(data);
    } else if (selected) {
      index_into_selected_children--;
    }
  }

  return FALSE;
}

gboolean SelectAllSelection(AtkSelection* selection) {
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  bool success = true;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AtkObjectToAXPlatformNodeAuraLinux(obj->ChildAtIndex(i));
    DCHECK(child);

    if (!child->SupportsSelectionWithAtkSelection())
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (selected)
      continue;

    AXActionData data;
    data.action = ax::mojom::Action::kDoDefault;
    success = success && child->GetDelegate()->AccessibilityPerformAction(data);
  }

  return success;
}

void Init(AtkSelectionIface* iface) {
  iface->add_selection = AddSelection;
  iface->clear_selection = ClearSelection;
  iface->ref_selection = RefSelection;
  iface->get_selection_count = GetSelectionCount;
  iface->is_child_selected = IsChildSelected;
  iface->remove_selection = RemoveSelection;
  iface->select_all_selection = SelectAllSelection;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_selection

namespace atk_table {

AtkObject* RefAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      if (AtkObject* atk_cell = cell->GetNativeViewAccessible()) {
        g_object_ref(atk_cell);
        return atk_cell;
      }
    }
  }

  return nullptr;
}

gint GetIndexAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* cell = obj->GetTableCell(row, column))
      return cell->GetTableCellIndex();
  }

  return -1;
}

gint GetColumnAtIndex(AtkTable* table, gint index) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* cell = obj->GetTableCell(index))
      return cell->GetTableColumn();
  }

  return -1;
}

gint GetRowAtIndex(AtkTable* table, gint index) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* cell = obj->GetTableCell(index))
      return cell->GetTableRow();
  }

  return -1;
}

gint GetNColumns(AtkTable* table) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table)))
    return obj->GetTableColumnCount();

  return 0;
}

gint GetNRows(AtkTable* table) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table)))
    return obj->GetTableRowCount();

  return 0;
}

gint GetColumnExtentAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* cell = obj->GetTableCell(row, column))
      return cell->GetTableColumnSpan();
  }

  return 0;
}

gint GetRowExtentAt(AtkTable* table, gint row, gint column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* cell = obj->GetTableCell(row, column))
      return cell->GetTableRowSpan();
  }

  return 0;
}

AtkObject* GetColumnHeader(AtkTable* table, gint column) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  // AtkTable supports only one column header object. So return the first one
  // we find. In the case of multiple headers, ATs can fall back on the column
  // description.
  std::vector<int32_t> ids = obj->GetDelegate()->GetColHeaderNodeIds(column);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        return atk_header;
      }
    }
  }

  return nullptr;
}

AtkObject* GetRowHeader(AtkTable* table, gint row) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  // AtkTable supports only one row header object. So return the first one
  // we find. In the case of multiple headers, ATs can fall back on the row
  // description.
  std::vector<int32_t> ids = obj->GetDelegate()->GetRowHeaderNodeIds(row);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        return atk_header;
      }
    }
  }

  return nullptr;
}

AtkObject* GetCaption(AtkTable* table) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table))) {
    if (auto* caption = obj->GetTableCaption())
      return caption->GetNativeViewAccessible();
  }

  return nullptr;
}

const gchar* GetColumnDescription(AtkTable* table, gint column) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  std::vector<int32_t> ids = obj->GetDelegate()->GetColHeaderNodeIds(column);
  return BuildDescriptionFromHeaders(obj->GetDelegate(), ids);
}

const gchar* GetRowDescription(AtkTable* table, gint row) {
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  std::vector<int32_t> ids = obj->GetDelegate()->GetRowHeaderNodeIds(row);
  return BuildDescriptionFromHeaders(obj->GetDelegate(), ids);
}

void Init(AtkTableIface* iface) {
  iface->ref_at = RefAt;
  iface->get_index_at = GetIndexAt;
  iface->get_column_at_index = GetColumnAtIndex;
  iface->get_row_at_index = GetRowAtIndex;
  iface->get_n_columns = GetNColumns;
  iface->get_n_rows = GetNRows;
  iface->get_column_extent_at = GetColumnExtentAt;
  iface->get_row_extent_at = GetRowExtentAt;
  iface->get_column_header = GetColumnHeader;
  iface->get_row_header = GetRowHeader;
  iface->get_caption = GetCaption;
  iface->get_column_description = GetColumnDescription;
  iface->get_row_description = GetRowDescription;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_table

// The ATK table cell interface was added in ATK 2.12.
#if defined(ATK_212)

namespace atk_table_cell {

gint GetColumnSpan(AtkTableCell* cell) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell)))
    return obj->GetTableColumnSpan();

  return 0;
}

GPtrArray* GetColumnHeaderCells(AtkTableCell* cell) {
  GPtrArray* array = g_ptr_array_new();

  // AtkTableCell is implemented on cells, row headers, and column headers.
  // Calling GetColHeaderNodeIds() on a column header cell will include that
  // column header, along with any other column headers in the column which
  // may or may not describe the header cell in question. Therefore, just return
  // headers for non-header cells.
  if (atk_object_get_role(ATK_OBJECT(cell)) != ATK_ROLE_TABLE_CELL)
    return array;

  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    if (auto* table = obj->GetTable()) {
      std::vector<int32_t> ids =
          table->GetDelegate()->GetColHeaderNodeIds(obj->GetTableColumn());
      IdsToGPtrArray(table->GetDelegate(), ids, array);
    }
  }

  return array;
}

gboolean GetCellPosition(AtkTableCell* cell, gint* row, gint* column) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    *row = obj->GetTableRow();
    *column = obj->GetTableColumn();
    return true;
  }

  return false;
}

gint GetRowSpan(AtkTableCell* cell) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell)))
    return obj->GetTableRowSpan();

  return 0;
}

GPtrArray* GetRowHeaderCells(AtkTableCell* cell) {
  GPtrArray* array = g_ptr_array_new();

  // AtkTableCell is implemented on cells, row headers, and column headers.
  // Calling GetRowHeaderNodeIds() on a row header cell will include that
  // row header, along with any other row headers in the row which may or
  // may not describe the header cell in question. Therefore, just return
  // headers for non-header cells.
  if (atk_object_get_role(ATK_OBJECT(cell)) != ATK_ROLE_TABLE_CELL)
    return array;

  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    if (auto* table = obj->GetTable()) {
      std::vector<int32_t> ids =
          table->GetDelegate()->GetRowHeaderNodeIds(obj->GetTableRow());
      IdsToGPtrArray(table->GetDelegate(), ids, array);
    }
  }

  return array;
}

AtkObject* GetTable(AtkTableCell* cell) {
  if (auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(cell))) {
    if (auto* table = obj->GetTable())
      return table->GetNativeViewAccessible();
  }

  return nullptr;
}

void Init(AtkTableCellIface* iface) {
  iface->get_column_span = GetColumnSpan;
  iface->get_column_header_cells = GetColumnHeaderCells;
  iface->get_position = GetCellPosition;
  iface->get_row_span = GetRowSpan;
  iface->get_row_header_cells = GetRowHeaderCells;
  iface->get_table = GetTable;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_table_cell

#endif  // ifdef ATK_212

namespace atk_object {

gpointer kAXPlatformNodeAuraLinuxParentClass = nullptr;

const gchar* GetName(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  ax::mojom::NameFrom name_from = obj->GetData().GetNameFrom();
  if (obj->GetStringAttribute(ax::mojom::StringAttribute::kName).empty() &&
      name_from != ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
    return nullptr;

  obj->accessible_name_ =
      obj->GetStringAttribute(ax::mojom::StringAttribute::kName);
  return obj->accessible_name_.c_str();
}

const gchar* GetDescription(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

gint GetIndexInParent(AtkObject* atk_object) {
  AtkObject* parent = atk_object_get_parent(atk_object);
  if (!parent)
    return -1;

  int n_children = atk_object_get_n_accessible_children(parent);
  for (int i = 0; i < n_children; i++) {
    AtkObject* child = atk_object_ref_accessible_child(parent, i);
    g_object_unref(child);
    if (child == atk_object)
      return i;
  }

  return -1;
}

AtkObject* GetParent(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetParent();
}

gint GetNChildren(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->GetChildCount();
}

AtkObject* RefChild(AtkObject* atk_object, gint index) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->ChildAtIndex(index);
  if (result)
    g_object_ref(result);
  return result;
}

AtkRelationSet* RefRelationSet(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return atk_relation_set_new();
  return obj->GetAtkRelations();
}

AtkAttributeSet* GetAttributes(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetAtkAttributes();
}

AtkRole GetRole(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->GetAtkRole();
}

AtkStateSet* RefStateSet(AtkObject* atk_object) {
  AtkStateSet* atk_state_set =
      ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
          ->ref_state_set(atk_object);

  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFUNCT);
  } else {
    obj->GetAtkState(atk_state_set);
  }
  return atk_state_set;
}
void Initialize(AtkObject* atk_object, gpointer data) {
  if (ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->initialize) {
    ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
        ->initialize(atk_object, data);
  }

  AX_PLATFORM_NODE_AURALINUX(atk_object)->m_object =
      reinterpret_cast<AXPlatformNodeAuraLinux*>(data);
}

void Finalize(GObject* atk_object) {
  G_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->finalize(atk_object);
}

void ClassInit(gpointer class_pointer, gpointer /* class_data */) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(class_pointer);
  kAXPlatformNodeAuraLinuxParentClass = g_type_class_peek_parent(gobject_class);
  gobject_class->finalize = Finalize;

  AtkObjectClass* atk_object_class = ATK_OBJECT_CLASS(gobject_class);
  atk_object_class->initialize = Initialize;
  atk_object_class->get_name = GetName;
  atk_object_class->get_description = GetDescription;
  atk_object_class->get_parent = GetParent;
  atk_object_class->get_n_children = GetNChildren;
  atk_object_class->ref_child = RefChild;
  atk_object_class->get_role = GetRole;
  atk_object_class->ref_state_set = RefStateSet;
  atk_object_class->get_index_in_parent = GetIndexInParent;
  atk_object_class->ref_relation_set = RefRelationSet;
  atk_object_class->get_attributes = GetAttributes;
}

GType GetType() {
  AXPlatformNodeAuraLinux::EnsureGTypeInit();

  static volatile gsize type_volatile = 0;
  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo type_info = {
        sizeof(AXPlatformNodeAuraLinuxClass),  // class_size
        nullptr,                               // base_init
        nullptr,                               // base_finalize
        atk_object::ClassInit,
        nullptr,                                // class_finalize
        nullptr,                                // class_data
        sizeof(AXPlatformNodeAuraLinuxObject),  // instance_size
        0,                                      // n_preallocs
        nullptr,                                // instance_init
        nullptr                                 // value_table
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "AXPlatformNodeAuraLinux", &type_info, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

void Detach(AXPlatformNodeAuraLinuxObject* atk_object) {
  if (!atk_object->m_object)
    return;

  atk_object->m_object = nullptr;
  atk_object_notify_state_change(ATK_OBJECT(atk_object), ATK_STATE_DEFUNCT,
                                 TRUE);
}

}  //  namespace atk_object

}  // namespace

void AXPlatformNodeAuraLinux::EnsureGTypeInit() {
#if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (UNLIKELY(first_time)) {
    g_type_init();
    first_time = false;
  }
#endif
}

int AXPlatformNodeAuraLinux::GetGTypeInterfaceMask() {
  int interface_mask = 0;

  // Component interface is always supported.
  interface_mask |= 1 << ATK_COMPONENT_INTERFACE;

  // Action interface is basic one. It just relays on executing default action
  // for each object.
  interface_mask |= 1 << ATK_ACTION_INTERFACE;

  // TODO(accessibility): We should only expose this for some elements, but
  // it might be better to do this after exposing the hypertext interface
  // as well.
  interface_mask |= 1 << ATK_TEXT_INTERFACE;

  if (!IsPlainTextField() && !IsChildOfLeaf())
    interface_mask |= 1 << ATK_HYPERTEXT_INTERFACE;

  // Value Interface
  AtkRole role = GetAtkRole();
  if (IsRangeValueSupported(GetData())) {
    interface_mask |= 1 << ATK_VALUE_INTERFACE;
  }

  // Document Interface
  if (role == ATK_ROLE_DOCUMENT_WEB)
    interface_mask |= 1 << ATK_DOCUMENT_INTERFACE;

  // Image Interface
  if (role == ATK_ROLE_IMAGE || role == ATK_ROLE_IMAGE_MAP)
    interface_mask |= 1 << ATK_IMAGE_INTERFACE;

  // HyperlinkImpl interface
  if (role == ATK_ROLE_LINK)
    interface_mask |= 1 << ATK_HYPERLINK_INTERFACE;

  if (role == ATK_ROLE_FRAME)
    interface_mask |= 1 << ATK_WINDOW_INTERFACE;

  if (IsContainerWithSelectableChildren(GetData().role))
    interface_mask |= 1 << ATK_SELECTION_INTERFACE;

  if (role == ATK_ROLE_TABLE)
    interface_mask |= 1 << ATK_TABLE_INTERFACE;

  // Because the TableCell Interface is only supported in ATK version 2.12 and
  // later, GetAccessibilityGType has a runtime check to verify we have a recent
  // enough version. If we don't, GetAccessibilityGType will exclude
  // AtkTableCell from the supported interfaces and none of its methods or
  // properties will be exposed to assistive technologies.
  if (role == ATK_ROLE_TABLE_CELL || role == ATK_ROLE_COLUMN_HEADER ||
      role == ATK_ROLE_ROW_HEADER)
    interface_mask |= 1 << ATK_TABLE_CELL_INTERFACE;

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

  const char* atk_type_name = GetUniqueAccessibilityGTypeName(interface_mask_);
  GType type = g_type_from_name(atk_type_name);
  if (type)
    return type;

  type = g_type_register_static(AX_PLATFORM_NODE_AURALINUX_TYPE, atk_type_name,
                                &type_info, GTypeFlags(0));
  if (interface_mask_ & (1 << ATK_COMPONENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &atk_component::Info);
  if (interface_mask_ & (1 << ATK_ACTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_ACTION, &atk_action::Info);
  if (interface_mask_ & (1 << ATK_DOCUMENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_DOCUMENT, &atk_document::Info);
  if (interface_mask_ & (1 << ATK_IMAGE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_IMAGE, &atk_image::Info);
  if (interface_mask_ & (1 << ATK_VALUE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_VALUE, &atk_value::Info);
  if (interface_mask_ & (1 << ATK_HYPERLINK_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_HYPERLINK_IMPL,
                                &atk_hyperlink::Info);
  if (interface_mask_ & (1 << ATK_HYPERTEXT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_HYPERTEXT, &atk_hypertext::Info);
  if (interface_mask_ & (1 << ATK_TEXT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_TEXT, &atk_text::Info);
  if (interface_mask_ & (1 << ATK_WINDOW_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_WINDOW, &atk_window::Info);
  if (interface_mask_ & (1 << ATK_SELECTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_SELECTION, &atk_selection::Info);
  if (interface_mask_ & (1 << ATK_TABLE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_TABLE, &atk_table::Info);

  // Run-time check to ensure AtkTableCell is supported (requires ATK 2.12).
  if (LoadTableCellMethods() &&
      interface_mask_ & (1 << ATK_TABLE_CELL_INTERFACE))
    g_type_add_interface_static(type, cell_get_type(), &atk_table_cell::Info);

  return type;
}

AtkObject* AXPlatformNodeAuraLinux::CreateAtkObject() {
  EnsureGTypeInit();
  interface_mask_ = GetGTypeInterfaceMask();
  GType type = GetAccessibilityGType();
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, nullptr));

  atk_object_initialize(atk_object, this);

  return ATK_OBJECT(atk_object);
}

void AXPlatformNodeAuraLinux::DestroyAtkObjects() {
  if (atk_hyperlink_) {
    ax_platform_atk_hyperlink_set_object(
        AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), nullptr);
    g_object_unref(atk_hyperlink_);
    atk_hyperlink_ = nullptr;
  }

  if (atk_object_) {
    if (atk_object_ == g_current_focused)
      g_current_focused = nullptr;
    atk_object::Detach(AX_PLATFORM_NODE_AURALINUX(atk_object_));

    g_object_unref(atk_object_);
    atk_object_ = nullptr;
  }
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
void AXPlatformNodeAuraLinux::SetApplication(AXPlatformNode* application) {
  g_root_application = application;
}

// static
AXPlatformNode* AXPlatformNodeAuraLinux::application() {
  return g_root_application;
}

// static
void AXPlatformNodeAuraLinux::StaticInitialize() {
  AtkUtilAuraLinux::GetInstance()->InitializeAsync();
}

AtkRole AXPlatformNodeAuraLinux::GetAtkRole() {
  switch (GetData().role) {
    case ax::mojom::Role::kAlert:
      return ATK_ROLE_ALERT;
    case ax::mojom::Role::kAlertDialog:
      return ATK_ROLE_DIALOG;
    case ax::mojom::Role::kAnchor:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kAnnotation:
      // TODO(accessibility) Panels are generally for containers of widgets.
      // This should probably be a section (if a container) or static if text.
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kApplication:
      // Only use ATK_ROLE_APPLICATION for elements with no parent, since it
      // is only for top level app windows and not ARIA applications.
      if (!GetParent()) {
        return ATK_ROLE_APPLICATION;
      } else {
        return ATK_ROLE_EMBEDDED;
      }
    case ax::mojom::Role::kArticle:
      return ATK_ROLE_ARTICLE;
    case ax::mojom::Role::kAudio:
      return ATK_ROLE_AUDIO;
    case ax::mojom::Role::kBanner:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kBlockquote:
      return ATK_ROLE_BLOCK_QUOTE;
    case ax::mojom::Role::kCaret:
      return ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kButton:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kCanvas:
      return ATK_ROLE_CANVAS;
    case ax::mojom::Role::kCaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kCell:
      return ATK_ROLE_TABLE_CELL;
    case ax::mojom::Role::kCheckBox:
      return ATK_ROLE_CHECK_BOX;
    case ax::mojom::Role::kSwitch:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kColorWell:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kColumn:
      return ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kColumnHeader:
      return ATK_ROLE_COLUMN_HEADER;
    case ax::mojom::Role::kComboBoxGrouping:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComboBoxMenuButton:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComplementary:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
      // TODO(accessibility) https://github.com/w3c/html-aam/issues/141
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDate:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDateTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDescriptionListDetail:
      return ATK_ROLE_DESCRIPTION_VALUE;
    case ax::mojom::Role::kDescriptionList:
      return ATK_ROLE_DESCRIPTION_LIST;
    case ax::mojom::Role::kDescriptionListTerm:
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kDetails:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kDialog:
      return ATK_ROLE_DIALOG;
    case ax::mojom::Role::kDirectory:
      return ATK_ROLE_LIST;
    case ax::mojom::Role::kDisclosureTriangle:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kDocCover:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocTip:
      return ATK_ROLE_COMMENT;
    case ax::mojom::Role::kDocFootnote:
      return kAtkFootnoteRole;
    case ax::mojom::Role::kDocPageBreak:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocToc:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kDocSubtitle:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kDocument:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kEmbeddedObject:
      return ATK_ROLE_EMBEDDED;
    case ax::mojom::Role::kForm:
      // TODO(accessibility) Forms which lack an accessible name are no longer
      // exposed as forms. http://crbug.com/874384. Forms which have accessible
      // names should be exposed as ATK_ROLE_LANDMARK according to Core AAM.
      return ATK_ROLE_FORM;
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kFeed:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGenericContainer:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kGraphicsDocument:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kGraphicsObject:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGraphicsSymbol:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kHeading:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
      return ATK_ROLE_INTERNAL_FRAME;
    case ax::mojom::Role::kIgnored:
      return ATK_ROLE_REDUNDANT_OBJECT;
    case ax::mojom::Role::kImage:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kImageMap:
      return ATK_ROLE_IMAGE_MAP;
    case ax::mojom::Role::kInputTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kLabelText:
      return ATK_ROLE_LABEL;
    case ax::mojom::Role::kLegend:
      return ATK_ROLE_LABEL;
    // Layout table objects are treated the same as Role::kGenericContainer.
    case ax::mojom::Role::kLayoutTable:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableCell:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableColumn:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableRow:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLineBreak:
      // TODO(Accessibility) Having a separate accessible object for line breaks
      // is inconsistent with other implementations. http://crbug.com/873144#c1.
      return ATK_ROLE_TEXT;
    case ax::mojom::Role::kLink:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kList:
      return ATK_ROLE_LIST;
    case ax::mojom::Role::kListBox:
      return ATK_ROLE_LIST_BOX;
    // TODO(Accessibility) Use ATK_ROLE_MENU_ITEM inside a combo box, see how
    // ax_platform_node_win.cc code does this.
    case ax::mojom::Role::kListBoxOption:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kListGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kListItem:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kListMarker:
      // TODO(Accessibility) Having a separate accessible object for the marker
      // is inconsistent with other implementations. http://crbug.com/873144.
      return kStaticRole;
    case ax::mojom::Role::kLog:
      return ATK_ROLE_LOG;
    case ax::mojom::Role::kMain:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kMark:
      return kStaticRole;
    case ax::mojom::Role::kMath:
      return ATK_ROLE_MATH;
    case ax::mojom::Role::kMarquee:
      return ATK_ROLE_MARQUEE;
    case ax::mojom::Role::kMenu:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuButton:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuBar:
      return ATK_ROLE_MENU_BAR;
    case ax::mojom::Role::kMenuItem:
      return ATK_ROLE_MENU_ITEM;
    case ax::mojom::Role::kMenuItemCheckBox:
      return ATK_ROLE_CHECK_MENU_ITEM;
    case ax::mojom::Role::kMenuItemRadio:
      return ATK_ROLE_RADIO_MENU_ITEM;
    case ax::mojom::Role::kMenuListPopup:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuListOption:
      return ATK_ROLE_MENU_ITEM;
    case ax::mojom::Role::kMeter:
      return ATK_ROLE_LEVEL_BAR;
    case ax::mojom::Role::kNavigation:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kNote:
      return ATK_ROLE_COMMENT;
    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kScrollView:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kParagraph:
      return ATK_ROLE_PARAGRAPH;
    case ax::mojom::Role::kPopUpButton: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "select")
        return ATK_ROLE_COMBO_BOX;
      return ATK_ROLE_PUSH_BUTTON;
    }
    case ax::mojom::Role::kPre:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kProgressIndicator:
      return ATK_ROLE_PROGRESS_BAR;
    case ax::mojom::Role::kRadioButton:
      return ATK_ROLE_RADIO_BUTTON;
    case ax::mojom::Role::kRadioGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return ATK_ROLE_SECTION;
      } else {
        // Use ARIA mapping.
        return ATK_ROLE_LANDMARK;
      }
    }
    case ax::mojom::Role::kRootWebArea:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kRow:
      return ATK_ROLE_TABLE_ROW;
    case ax::mojom::Role::kRowHeader:
      return ATK_ROLE_ROW_HEADER;
    case ax::mojom::Role::kRuby:
      return kStaticRole;
    case ax::mojom::Role::kScrollBar:
      return ATK_ROLE_SCROLL_BAR;
    case ax::mojom::Role::kSearch:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSliderThumb:
      return ATK_ROLE_SLIDER;
    case ax::mojom::Role::kSpinButton:
      return ATK_ROLE_SPIN_BUTTON;
    case ax::mojom::Role::kSplitter:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kStaticText: {
      switch (static_cast<ax::mojom::TextPosition>(
          GetIntAttribute(ax::mojom::IntAttribute::kTextPosition))) {
        case ax::mojom::TextPosition::kSubscript:
          return kSubscriptRole;
        case ax::mojom::TextPosition::kSuperscript:
          return kSuperscriptRole;
        default:
          break;
      }
      return ATK_ROLE_TEXT;
    }
    case ax::mojom::Role::kStatus:
      return ATK_ROLE_STATUSBAR;
    case ax::mojom::Role::kSvgRoot:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kTab:
      return ATK_ROLE_PAGE_TAB;
    case ax::mojom::Role::kTable:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kTableHeaderContainer:
      // TODO(accessibility) This mapping is correct, but it doesn't seem to be
      // used. We don't necessarily want to always expose these containers, but
      // we must do so if they are focusable. http://crbug.com/874043
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kTabList:
      return ATK_ROLE_PAGE_TAB_LIST;
    case ax::mojom::Role::kTabPanel:
      return ATK_ROLE_SCROLL_PANE;
    case ax::mojom::Role::kTerm:
      // TODO(accessibility) This mapping should also be applied to the dfn
      // element. http://crbug.com/874411
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kTitleBar:
      return ATK_ROLE_TITLE_BAR;
    case ax::mojom::Role::kInlineTextBox:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      if (GetData().HasState(ax::mojom::State::kProtected))
        return ATK_ROLE_PASSWORD_TEXT;
      // TODO(crbug.com/865101) Use
      // GetData().HasState(ax::mojom::State::kAutofillAvailable) instead of
      // IsFocusedInputWithSuggestions()
      if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete)
               .empty() ||
          IsFocusedInputWithSuggestions()) {
        return ATK_ROLE_AUTOCOMPLETE;
      }
      return ATK_ROLE_ENTRY;
    case ax::mojom::Role::kTextFieldWithComboBox:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kTime:
      return kStaticRole;
    case ax::mojom::Role::kTimer:
      return ATK_ROLE_TIMER;
    case ax::mojom::Role::kToggleButton:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kToolbar:
      return ATK_ROLE_TOOL_BAR;
    case ax::mojom::Role::kTooltip:
      return ATK_ROLE_TOOL_TIP;
    case ax::mojom::Role::kTree:
      return ATK_ROLE_TREE;
    case ax::mojom::Role::kTreeItem:
      return ATK_ROLE_TREE_ITEM;
    case ax::mojom::Role::kTreeGrid:
      return ATK_ROLE_TREE_TABLE;
    case ax::mojom::Role::kVideo:
      return ATK_ROLE_VIDEO;
    case ax::mojom::Role::kWebArea:
    case ax::mojom::Role::kWebView:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kWindow:
      // In ATK elements with ATK_ROLE_FRAME are windows with titles and
      // buttons, while those with ATK_ROLE_WINDOW are windows without those
      // elements.
      return ATK_ROLE_FRAME;
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kDesktop:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kFigcaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kUnknown:
      return ATK_ROLE_REDUNDANT_OBJECT;
  }
}

void AXPlatformNodeAuraLinux::GetAtkState(AtkStateSet* atk_state_set) {
  AXNodeData data = GetData();

  bool menu_active = !GetActiveMenus().empty();
  if (!menu_active && atk_object_ == g_active_top_level_frame)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);
  if (menu_active &&
      FindAtkObjectParentFrame(GetActiveMenus().back()) == atk_object_)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);

  if (data.HasState(ax::mojom::State::kCollapsed))
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
  if (data.HasState(ax::mojom::State::kDefault))
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFAULT);
  if (data.HasState(ax::mojom::State::kEditable) &&
      data.GetRestriction() != ax::mojom::Restriction::kReadOnly) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EDITABLE);
  }
  if (data.HasState(ax::mojom::State::kExpanded)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDED);
  }
  if (data.HasState(ax::mojom::State::kFocusable) ||
      SelectionAndFocusAreTheSame())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSABLE);
  if (data.HasState(ax::mojom::State::kHorizontal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HORIZONTAL);
  if (!data.HasState(ax::mojom::State::kInvisible)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISIBLE);
    if (!delegate_->IsOffscreen())
      atk_state_set_add_state(atk_state_set, ATK_STATE_SHOWING);
  }
  if (data.HasState(ax::mojom::State::kMultiselectable))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MULTISELECTABLE);
  if (data.HasState(ax::mojom::State::kRequired))
    atk_state_set_add_state(atk_state_set, ATK_STATE_REQUIRED);
  if (data.HasState(ax::mojom::State::kVertical))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VERTICAL);
  if (data.HasState(ax::mojom::State::kVisited))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISITED);
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      data.GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse))
    atk_state_set_add_state(atk_state_set, ATK_STATE_INVALID_ENTRY);
#if defined(ATK_216)
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState) &&
      data.role != ax::mojom::Role::kToggleButton) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_CHECKABLE);
  }
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HAS_POPUP);
#endif
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    atk_state_set_add_state(atk_state_set, ATK_STATE_BUSY);
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MODAL);
  if (data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE);
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTED);

  if (IsPlainTextField() || IsRichTextField()) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE_TEXT);
    if (data.HasState(ax::mojom::State::kMultiline))
      atk_state_set_add_state(atk_state_set, ATK_STATE_MULTI_LINE);
    else
      atk_state_set_add_state(atk_state_set, ATK_STATE_SINGLE_LINE);
  }

  // TODO(crbug.com/865101) Use
  // GetData().HasState(ax::mojom::State::kAutofillAvailable) instead of
  // IsFocusedInputWithSuggestions()
  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      IsFocusedInputWithSuggestions())
    atk_state_set_add_state(atk_state_set, ATK_STATE_SUPPORTS_AUTOCOMPLETION);

  // Checked state
  const auto checked_state = GetData().GetCheckedState();
  if (checked_state == ax::mojom::CheckedState::kTrue ||
      checked_state == ax::mojom::CheckedState::kMixed) {
    atk_state_set_add_state(atk_state_set, GetAtkStateTypeForCheckableNode());
  }

  switch (GetData().GetRestriction()) {
    case ax::mojom::Restriction::kNone:
      atk_state_set_add_state(atk_state_set, ATK_STATE_ENABLED);
      atk_state_set_add_state(atk_state_set, ATK_STATE_SENSITIVE);
      break;
    case ax::mojom::Restriction::kReadOnly:
#if defined(ATK_216)
      atk_state_set_add_state(atk_state_set, ATK_STATE_READ_ONLY);
#endif
      break;
    default:
      break;
  }

  if (delegate_->GetFocus() == GetNativeViewAccessible())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSED);
}

struct AtkIntRelation {
  ax::mojom::IntAttribute attribute;
  AtkRelationType relation;
  base::Optional<AtkRelationType> reverse_relation;
};

static AtkIntRelation kIntRelations[] = {
    {ax::mojom::IntAttribute::kMemberOfId, ATK_RELATION_MEMBER_OF,
     base::nullopt},
#if defined(ATK_226)
    {ax::mojom::IntAttribute::kDetailsId, ATK_RELATION_DETAILS,
     ATK_RELATION_DETAILS_FOR},
    {ax::mojom::IntAttribute::kErrormessageId, ATK_RELATION_ERROR_MESSAGE,
     ATK_RELATION_ERROR_FOR},
#endif
};

struct AtkIntListRelation {
  ax::mojom::IntListAttribute attribute;
  AtkRelationType relation;
  base::Optional<AtkRelationType> reverse_relation;
};

static AtkIntListRelation kIntListRelations[] = {
    {ax::mojom::IntListAttribute::kControlsIds, ATK_RELATION_CONTROLLER_FOR,
     ATK_RELATION_CONTROLLED_BY},
    {ax::mojom::IntListAttribute::kDescribedbyIds, ATK_RELATION_DESCRIBED_BY,
     ATK_RELATION_DESCRIPTION_FOR},
    {ax::mojom::IntListAttribute::kFlowtoIds, ATK_RELATION_FLOWS_TO,
     ATK_RELATION_FLOWS_FROM},
    {ax::mojom::IntListAttribute::kLabelledbyIds, ATK_RELATION_LABELLED_BY,
     ATK_RELATION_LABEL_FOR},
};

void AXPlatformNodeAuraLinux::AddRelationToSet(AtkRelationSet* relation_set,
                                               AtkRelationType relation,
                                               AXPlatformNode* target) {
  DCHECK(target);

  // Avoid adding self-referential relations.
  if (target == this)
    return;

  // If we were compiled with a newer version of ATK than the runtime version,
  // it's possible that we might try to add a relation that doesn't exist in
  // the runtime version of the AtkRelationType enum. This will cause a runtime
  // error, so return early here if we are about to do that.
  static base::Optional<int> max_relation_type = base::nullopt;
  if (!max_relation_type.has_value()) {
    GEnumClass* enum_class =
        G_ENUM_CLASS(g_type_class_ref(atk_relation_type_get_type()));
    max_relation_type = enum_class->maximum;
    g_type_class_unref(enum_class);
  }
  if (relation > max_relation_type.value())
    return;

  atk_relation_set_add_relation_by_type(relation_set, relation,
                                        target->GetNativeViewAccessible());
}

AtkRelationSet* AXPlatformNodeAuraLinux::GetAtkRelations() {
  AtkRelationSet* relation_set = atk_relation_set_new();

  if (embedded_document_) {
    atk_relation_set_add_relation_by_type(relation_set, ATK_RELATION_EMBEDS,
                                          embedded_document_);
  }

  if (embedding_window_) {
    atk_relation_set_add_relation_by_type(
        relation_set, ATK_RELATION_EMBEDDED_BY, embedding_window_);
  }

  // For each possible relation defined by an IntAttribute, we test that
  // attribute and then look for reverse relations. AddRelationToSet handles
  // discarding self-referential relations.
  for (unsigned i = 0; i < G_N_ELEMENTS(kIntRelations); i++) {
    const AtkIntRelation& relation = kIntRelations[i];

    if (AXPlatformNode* target =
            GetDelegate()->GetTargetNodeForRelation(relation.attribute))
      AddRelationToSet(relation_set, relation.relation, target);

    if (!relation.reverse_relation.has_value())
      continue;

    std::set<AXPlatformNode*> target_ids =
        GetDelegate()->GetReverseRelations(relation.attribute);
    for (AXPlatformNode* target : target_ids) {
      AddRelationToSet(relation_set, relation.reverse_relation.value(), target);
    }
  }

  // Now we do the same for each possible relation defined by an
  // IntListAttribute. In this case we need to handle each target in the list.
  for (unsigned i = 0; i < G_N_ELEMENTS(kIntListRelations); i++) {
    const AtkIntListRelation& relation = kIntListRelations[i];

    std::set<AXPlatformNode*> targets =
        GetDelegate()->GetTargetNodesForRelation(relation.attribute);
    for (AXPlatformNode* target : targets)
      AddRelationToSet(relation_set, relation.relation, target);

    if (!relation.reverse_relation.has_value())
      continue;

    std::set<AXPlatformNode*> reverse_target_ids =
        GetDelegate()->GetReverseRelations(relation.attribute);
    for (AXPlatformNode* target : reverse_target_ids) {
      AddRelationToSet(relation_set, relation.reverse_relation.value(), target);
    }
  }

  return relation_set;
}

AXPlatformNodeAuraLinux::AXPlatformNodeAuraLinux() = default;

AXPlatformNodeAuraLinux::~AXPlatformNodeAuraLinux() {
  if (g_current_selected == this)
    g_current_selected = nullptr;

  DestroyAtkObjects();

  SetWeakGPtrToAtkObject(&embedded_document_, nullptr);
  SetWeakGPtrToAtkObject(&embedding_window_, nullptr);
}

void AXPlatformNodeAuraLinux::Destroy() {
  DestroyAtkObjects();
  AXPlatformNodeBase::Destroy();
}

void AXPlatformNodeAuraLinux::Init(AXPlatformNodeDelegate* delegate) {
  // Initialize ATK.
  AXPlatformNodeBase::Init(delegate);
  DataChanged();
}

void AXPlatformNodeAuraLinux::DataChanged() {
  if (atk_object_) {
    // If the object's role changes and that causes its
    // interface mask to change, we need to create a new
    // AtkObject for it.
    int interface_mask = GetGTypeInterfaceMask();
    if (interface_mask != interface_mask_)
      DestroyAtkObjects();
  }

  if (!atk_object_) {
    atk_object_ = CreateAtkObject();
  }
}

void AXPlatformNodeAuraLinux::AddAccessibilityTreeProperties(
    base::DictionaryValue* dict) {
  AtkRole role = GetAtkRole();
  if (role != ATK_ROLE_UNKNOWN) {
    int role_index = static_cast<int>(role);
    dict->SetString("role", kRoleNames[role_index]);
  }
  const gchar* name = atk_object_get_name(atk_object_);
  if (name)
    dict->SetString("name", std::string(name));
  const gchar* description = atk_object_get_description(atk_object_);
  if (description)
    dict->SetString("description", std::string(description));

  AtkStateSet* state_set = atk_object_ref_state_set(atk_object_);
  auto states = std::make_unique<base::ListValue>();
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type))
      states->AppendString(atk_state_type_get_name(state_type));
  }
  dict->Set("states", std::move(states));

  AtkRelationSet* relation_set = atk_object_ref_relation_set(atk_object_);
  auto relations = std::make_unique<base::ListValue>();
  for (int i = ATK_RELATION_NULL; i < ATK_RELATION_LAST_DEFINED; i++) {
    AtkRelationType relation_type = static_cast<AtkRelationType>(i);
    if (atk_relation_set_contains(relation_set, relation_type))
      relations->AppendString(atk_relation_type_get_name(relation_type));
  }
  dict->Set("relations", std::move(relations));

  AtkAttributeSet* attributes = atk_object_get_attributes(atk_object_);
  for (AtkAttributeSet* attr = attributes; attr; attr = attr->next) {
    AtkAttribute* attribute = static_cast<AtkAttribute*>(attr->data);
    dict->SetString(attribute->name, attribute->value);
  }
  atk_attribute_set_free(attributes);

  // Properties obtained via AtkValue.
  auto value_properties = std::make_unique<base::ListValue>();
  if (ATK_IS_VALUE(atk_object_)) {
    AtkValue* value = ATK_VALUE(atk_object_);
    GValue current = G_VALUE_INIT;
    g_value_init(&current, G_TYPE_FLOAT);
    atk_value_get_current_value(value, &current);
    value_properties->AppendString(
        base::StringPrintf("current=%f", g_value_get_float(&current)));

    GValue minimum = G_VALUE_INIT;
    g_value_init(&minimum, G_TYPE_FLOAT);
    atk_value_get_minimum_value(value, &minimum);
    value_properties->AppendString(
        base::StringPrintf("minimum=%f", g_value_get_float(&minimum)));

    GValue maximum = G_VALUE_INIT;
    g_value_init(&maximum, G_TYPE_FLOAT);
    atk_value_get_maximum_value(value, &maximum);
    value_properties->AppendString(
        base::StringPrintf("maximum=%f", g_value_get_float(&maximum)));
  }
  dict->Set("value", std::move(value_properties));

  // Properties obtained via AtkTable.
  auto table_properties = std::make_unique<base::ListValue>();
  if (ATK_IS_TABLE(atk_object_)) {
    AtkTable* table = ATK_TABLE(atk_object_);

    // Column details.
    int n_cols = atk_table_get_n_columns(table);
    table_properties->AppendString(base::StringPrintf("cols=%i", n_cols));

    std::vector<std::string> col_headers;
    for (int i = 0; i < n_cols; i++) {
      std::string header = atk_table_get_column_description(table, i);
      if (!header.empty())
        col_headers.push_back(base::StringPrintf("'%s'", header.c_str()));
    }

    if (!col_headers.size())
      col_headers.push_back("NONE");

    table_properties->AppendString(base::StringPrintf(
        "headers=(%s);", base::JoinString(col_headers, ", ").c_str()));

    // Row details.
    int n_rows = atk_table_get_n_rows(table);
    table_properties->AppendString(base::StringPrintf("rows=%i", n_rows));

    std::vector<std::string> row_headers;
    for (int i = 0; i < n_rows; i++) {
      std::string header = atk_table_get_row_description(table, i);
      if (!header.empty())
        row_headers.push_back(base::StringPrintf("'%s'", header.c_str()));
    }

    if (!row_headers.size())
      row_headers.push_back("NONE");

    table_properties->AppendString(base::StringPrintf(
        "headers=(%s);", base::JoinString(row_headers, ", ").c_str()));

    // Caption details.
    AtkObject* caption = atk_table_get_caption(table);
    table_properties->AppendString(
        base::StringPrintf("caption=%s;", caption ? "true" : "false"));

    // Summarize information about the cells from the table's perspective here.
    std::vector<std::string> span_info;
    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        int row_span = atk_table_get_row_extent_at(table, r, c);
        int col_span = atk_table_get_column_extent_at(table, r, c);
        if (row_span != 1 || col_span != 1) {
          span_info.push_back(base::StringPrintf("cell at %i,%i: %ix%i", r, c,
                                                 row_span, col_span));
        }
      }
    }
    if (!span_info.size())
      span_info.push_back("all: 1x1");

    table_properties->AppendString(base::StringPrintf(
        "spans=(%s)", base::JoinString(span_info, ", ").c_str()));
  }

  dict->Set("table", std::move(table_properties));

  // Properties obtained via AtkTableCell, if possible. If we do not have at
  // least ATK 2.12, use the same logic in our AtkTableCell implementation so
  // that tests can still be run.
  auto cell_properties = std::make_unique<base::ListValue>();
  if (role == ATK_ROLE_TABLE_CELL || role == ATK_ROLE_COLUMN_HEADER ||
      role == ATK_ROLE_ROW_HEADER) {
    int row, col, row_span, col_span;
    GPtrArray* col_headers;
    GPtrArray* row_headers;
    if (cell_get_type) {
      AtkTableCell* cell = G_TYPE_CHECK_INSTANCE_CAST(
          (atk_object_), cell_get_type(), AtkTableCell);
      col_headers = get_column_header_cells(cell);
      row_headers = get_row_header_cells(cell);
      get_row_column_span(cell, &row, &col, &row_span, &col_span);
    } else {
      auto* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object_);
      row = obj->GetTableRow();
      col = obj->GetTableColumn();
      row_span = obj->GetTableRowSpan();
      col_span = obj->GetTableColumnSpan();
      col_headers = g_ptr_array_new();
      row_headers = g_ptr_array_new();
      if (role == ATK_ROLE_TABLE_CELL) {
        auto* delegate = obj->GetTable()->GetDelegate();
        std::vector<int32_t> col_header_ids =
            delegate->GetColHeaderNodeIds(col);
        std::vector<int32_t> row_header_ids =
            delegate->GetRowHeaderNodeIds(row);
        IdsToGPtrArray(delegate, col_header_ids, col_headers);
        IdsToGPtrArray(delegate, row_header_ids, row_headers);
      }
    }
    cell_properties->AppendString(
        base::StringPrintf("(row=%i, col=%i, row_span=%i, col_span=%i", row,
                           col, row_span, col_span));
    cell_properties->AppendString(
        base::StringPrintf("n_row_headers=%i, n_col_headers=%i)",
                           row_headers->len, col_headers->len));
  }
  dict->Set("cell", std::move(cell_properties));
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetNativeViewAccessible() {
  return atk_object_;
}

void AXPlatformNodeAuraLinux::OnCheckedStateChanged() {
  DCHECK(atk_object_);

  atk_object_notify_state_change(
      ATK_OBJECT(atk_object_), GetAtkStateTypeForCheckableNode(),
      GetData().GetCheckedState() != ax::mojom::CheckedState::kFalse);
}

void AXPlatformNodeAuraLinux::OnExpandedStateChanged(bool is_expanded) {
  // When a list box is expanded, it becomes visible. This means that it might
  // now have a different role (the role for hidden Views is kUnknown).  We
  // need to recreate the AtkObject in this case because a change in roles
  // might imply a change in ATK interfaces implemented.
  DataChanged();

  DCHECK(atk_object_);
  atk_object_notify_state_change(ATK_OBJECT(atk_object_), ATK_STATE_EXPANDED,
                                 is_expanded);
}

void AXPlatformNodeAuraLinux::OnMenuPopupStart() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object_);
  if (!parent_frame)
    return;

  // Exit early if kMenuPopupStart is sent multiple times for the same menu.
  std::vector<AtkObject*>& active_menus = GetActiveMenus();
  bool menu_already_open = !active_menus.empty();
  if (menu_already_open && active_menus.back() == atk_object_)
    return;

  // We also want to inform the AT that menu the is now showing. Normally this
  // event is not fired because the menu will be created with the
  // ATK_STATE_SHOWING already set to TRUE.
  atk_object_notify_state_change(atk_object_, ATK_STATE_SHOWING, TRUE);

  // We need to compute this before modifying the active menu stack.
  AtkObject* previous_active_frame = ComputeActiveTopLevelFrame();

  active_menus.push_back(atk_object_);

  // We exit early if the newly activated menu has the same AtkWindow as the
  // previous one.
  if (previous_active_frame == parent_frame)
    return;
  if (previous_active_frame) {
    g_signal_emit_by_name(previous_active_frame, "deactivate");
    atk_object_notify_state_change(previous_active_frame, ATK_STATE_ACTIVE,
                                   FALSE);
  }
  g_signal_emit_by_name(parent_frame, "activate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, TRUE);
}

void AXPlatformNodeAuraLinux::OnMenuPopupHide() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object_);
  if (!parent_frame)
    return;

  atk_object_notify_state_change(atk_object_, ATK_STATE_SHOWING, FALSE);

  // kMenuPopupHide may be called multiple times for the same menu, so only
  // remove it if our parent frame matches the most recently opened menu.
  std::vector<AtkObject*>& active_menus = GetActiveMenus();
  if (active_menus.empty())
    return;

  // When multiple levels of menu are closed at once, they may be hidden out
  // of order. When this happens, we just remove the open menu from the stack.
  if (active_menus.back() != atk_object_) {
    auto it =
        std::find(active_menus.rbegin(), active_menus.rend(), atk_object_);
    if (it != active_menus.rend()) {
      // We used a reverse iterator, so we need to convert it into a normal
      // iterator to use it for std::vector::erase(...).
      auto to_remove = --(it.base());
      active_menus.erase(to_remove);
    }
    return;
  }

  active_menus.pop_back();

  // We exit early if the newly activated menu has the same AtkWindow as the
  // previous one.
  AtkObject* new_active_item = ComputeActiveTopLevelFrame();
  if (new_active_item == parent_frame)
    return;
  g_signal_emit_by_name(parent_frame, "deactivate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, FALSE);
  if (new_active_item) {
    g_signal_emit_by_name(new_active_item, "activate");
    atk_object_notify_state_change(new_active_item, ATK_STATE_ACTIVE, TRUE);
  }
}

void AXPlatformNodeAuraLinux::OnMenuPopupEnd() {
  if (!GetActiveMenus().empty() && g_active_top_level_frame &&
      ComputeActiveTopLevelFrame() != g_active_top_level_frame) {
    g_signal_emit_by_name(g_active_top_level_frame, "activate");
    atk_object_notify_state_change(g_active_top_level_frame, ATK_STATE_ACTIVE,
                                   TRUE);
  }

  GetActiveMenus().clear();
}

void AXPlatformNodeAuraLinux::OnWindowActivated() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object_);
  if (!parent_frame || parent_frame == g_active_top_level_frame)
    return;

  SetActiveTopLevelFrame(parent_frame);

  g_signal_emit_by_name(parent_frame, "activate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, TRUE);

  // We also send a focus event for the currently focused element, so that
  // the user knows where the focus is when the toplevel window regains focus.
  if (g_current_focused &&
      IsFrameAncestorOfAtkObject(parent_frame, g_current_focused)) {
    g_signal_emit_by_name(g_current_focused, "focus-event", true);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, true);
  }
}

void AXPlatformNodeAuraLinux::OnWindowDeactivated() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object_);
  if (!parent_frame || parent_frame != g_active_top_level_frame)
    return;

  SetActiveTopLevelFrame(nullptr);

  g_signal_emit_by_name(parent_frame, "deactivate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, FALSE);
}

void AXPlatformNodeAuraLinux::OnFocused() {
  DCHECK(atk_object_);

  if (atk_object_get_role(atk_object_) == ATK_ROLE_FRAME) {
    OnWindowActivated();
    return;
  }

  if (atk_object_ == g_current_focused)
    return;

  if (g_current_focused) {
    g_signal_emit_by_name(g_current_focused, "focus-event", false);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, false);
  }

  g_current_focused = atk_object_;
  g_signal_emit_by_name(atk_object_, "focus-event", true);
  atk_object_notify_state_change(ATK_OBJECT(atk_object_), ATK_STATE_FOCUSED,
                                 true);
}

void AXPlatformNodeAuraLinux::OnSelected() {
  if (g_current_selected && !g_current_selected->GetData().GetBoolAttribute(
                                ax::mojom::BoolAttribute::kSelected)) {
    atk_object_notify_state_change(ATK_OBJECT(g_current_selected->atk_object_),
                                   ATK_STATE_SELECTED, false);
  }

  g_current_selected = this;
  if (ATK_IS_OBJECT(atk_object_)) {
    atk_object_notify_state_change(ATK_OBJECT(atk_object_), ATK_STATE_SELECTED,
                                   true);
  }

  if (SelectionAndFocusAreTheSame())
    OnFocused();
}

void AXPlatformNodeAuraLinux::OnSelectedChildrenChanged() {
  g_signal_emit_by_name(ATK_OBJECT(atk_object_), "selection-changed", true);
}

bool AXPlatformNodeAuraLinux::SelectionAndFocusAreTheSame() {
  if (AXPlatformNodeBase* container = GetSelectionContainer()) {
    ax::mojom::Role role = container->GetData().role;
    if (role == ax::mojom::Role::kMenuBar || role == ax::mojom::Role::kMenu)
      return true;
    if (role == ax::mojom::Role::kListBox &&
        !container->GetData().HasState(ax::mojom::State::kMultiselectable)) {
      return container->GetDelegate()->GetFocus() ==
             container->GetNativeViewAccessible();
    }
  }

  // TODO(accessibility): GetSelectionContainer returns nullptr when the current
  // object is a descendant of a select element with a size of 1. Intentional?
  // For now, handle that scenario here.
  //
  // If the selection is changing on a collapsed select element, focus remains
  // on the select element and not the newly-selected descendant.
  if (AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent())) {
    if (parent->GetData().role == ax::mojom::Role::kMenuListPopup)
      return !parent->GetData().HasState(ax::mojom::State::kInvisible);
  }

  return false;
}

void AXPlatformNodeAuraLinux::OnTextSelectionChanged() {
  if (HasCaret()) {
    g_signal_emit_by_name(atk_object_, "text-caret-moved",
                          atk_text_get_caret_offset(ATK_TEXT(atk_object_)));
  }
  g_signal_emit_by_name(atk_object_, "text-selection-changed");
}

bool AXPlatformNodeAuraLinux::SupportsSelectionWithAtkSelection() {
  return SupportsToggle(GetData().role) ||
         GetData().role == ax::mojom::Role::kListBoxOption;
}

void AXPlatformNodeAuraLinux::OnDescriptionChanged() {
  std::string description;
  GetStringAttribute(ax::mojom::StringAttribute::kDescription, &description);

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-description";
  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_STRING);
  g_value_set_string(&property_values.new_value, description.c_str());
  g_signal_emit_by_name(G_OBJECT(atk_object_),
                        "property-change::accessible-description",
                        &property_values, nullptr);
}

void AXPlatformNodeAuraLinux::OnValueChanged() {
  DCHECK(atk_object_);

  if (!IsRangeValueSupported(GetData()))
    return;

  float float_val;
  if (!GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, &float_val))
    return;

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-value";

  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_DOUBLE);
  g_value_set_double(&property_values.new_value,
                     static_cast<double>(float_val));
  g_signal_emit_by_name(G_OBJECT(atk_object_),
                        "property-change::accessible-value", &property_values,
                        nullptr);
}

void AXPlatformNodeAuraLinux::OnNameChanged() {
  std::string name = GetStringAttribute(ax::mojom::StringAttribute::kName);

  std::string previous_accessible_name = accessible_name_;
  // Calling atk_object_get_name will update the value of accessible_name_.
  if (!g_strcmp0(atk_object_get_name(atk_object_),
                 previous_accessible_name.c_str()))
    return;

  g_object_notify(G_OBJECT(atk_object_), "accessible-name");
}

void AXPlatformNodeAuraLinux::OnDocumentTitleChanged() {
  DCHECK(atk_object_);
  if (!g_active_top_level_frame)
    return;

  // We always want to notify on the top frame.
  AXPlatformNodeAuraLinux* window =
      AtkObjectToAXPlatformNodeAuraLinux(g_active_top_level_frame);
  window->OnNameChanged();
}

void AXPlatformNodeAuraLinux::OnInvalidStatusChanged() {
  DCHECK(atk_object_);
  atk_object_notify_state_change(
      ATK_OBJECT(atk_object_), ATK_STATE_INVALID_ENTRY,
      GetData().GetInvalidState() != ax::mojom::InvalidState::kFalse);
}

void AXPlatformNodeAuraLinux::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  switch (event_type) {
    // There are three types of messages that we receive for popup menus. Each
    // time a popup menu is shown, we get a kMenuPopupStart message. This
    // includes if the menu is hidden and then re-shown. When a menu is hidden
    // we receive the kMenuPopupHide message. Finally, when the entire menu is
    // closed we receive the kMenuPopupEnd message for the parent menu and all
    // of the submenus that were opened when navigating through the menu.
    case ax::mojom::Event::kMenuPopupEnd:
      OnMenuPopupEnd();
      break;
    case ax::mojom::Event::kMenuPopupHide:
      OnMenuPopupHide();
      break;
    case ax::mojom::Event::kMenuPopupStart:
      OnMenuPopupStart();
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      OnCheckedStateChanged();
      break;
    case ax::mojom::Event::kExpandedChanged:
      OnExpandedStateChanged(GetData().HasState(ax::mojom::State::kExpanded));
      break;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      OnFocused();
      break;
    case ax::mojom::Event::kSelection:
      OnSelected();
      // When changing tabs also fire a name changed event.
      if (GetData().role == ax::mojom::Role::kTab)
        OnDocumentTitleChanged();
      break;
    case ax::mojom::Event::kSelectedChildrenChanged:
      OnSelectedChildrenChanged();
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      OnTextSelectionChanged();
      break;
    case ax::mojom::Event::kValueChanged:
      OnValueChanged();
      break;
    case ax::mojom::Event::kInvalidStatusChanged:
      OnInvalidStatusChanged();
      break;
    case ax::mojom::Event::kWindowActivated:
      OnWindowActivated();
      break;
    case ax::mojom::Event::kWindowDeactivated:
      OnWindowDeactivated();
      break;
    case ax::mojom::Event::kLoadComplete:
    case ax::mojom::Event::kDocumentTitleChanged:
      // Sometimes, e.g. upon navigating away from the page, the tree is
      // rebuilt rather than modified. The kDocumentTitleChanged event occurs
      // prior to the rebuild and so is added on the previous root node. When
      // the tree is rebuilt and the old node removed, the events on the old
      // node are removed and no new kDocumentTitleChanged will be emitted. To
      // ensure we still fire the event, though, we also pay attention to
      // kLoadComplete.
      OnDocumentTitleChanged();
      break;
    default:
      break;
  }
}

void AXPlatformNodeAuraLinux::UpdateHypertext() {
  AXHypertext old_hypertext = hypertext_;
  base::OffsetAdjuster::Adjustments old_adjustments = GetHypertextAdjustments();

  UpdateComputedHypertext();
  text_unicode_adjustments_ = base::nullopt;

  if ((!GetData().HasState(ax::mojom::State::kEditable) ||
       GetData().GetRestriction() == ax::mojom::Restriction::kReadOnly) &&
      !IsInLiveRegion()) {
    return;
  }

  size_t shared_prefix, old_len, new_len;
  ComputeHypertextRemovedAndInserted(old_hypertext, &shared_prefix, &old_len,
                                     &new_len);

  DCHECK(atk_object_);
  DCHECK(ATK_IS_TEXT(atk_object_));

  if (old_len > 0) {
    base::string16 removed_substring =
        old_hypertext.hypertext.substr(shared_prefix, old_len);

    size_t shared_unicode_prefix = shared_prefix;
    base::OffsetAdjuster::AdjustOffset(old_adjustments, &shared_unicode_prefix);
    size_t shared_unicode_suffix = shared_prefix + old_len;
    base::OffsetAdjuster::AdjustOffset(old_adjustments, &shared_unicode_suffix);

    g_signal_emit_by_name(
        atk_object_, "text-remove",
        shared_unicode_prefix,                  // position of removal
        shared_unicode_suffix - shared_prefix,  // length of removal
        base::UTF16ToUTF8(removed_substring).c_str());
  }

  if (new_len > 0) {
    base::string16 inserted_substring =
        hypertext_.hypertext.substr(shared_prefix, new_len);
    size_t shared_unicode_prefix = UTF16ToUnicodeOffsetInText(shared_prefix);
    size_t shared_unicode_suffix =
        UTF16ToUnicodeOffsetInText(shared_prefix + new_len);
    g_signal_emit_by_name(
        atk_object_, "text-insert",
        shared_unicode_prefix,                          // position of insertion
        shared_unicode_suffix - shared_unicode_prefix,  // length of insertion
        base::UTF16ToUTF8(inserted_substring).c_str());
  }
}

const AXHypertext& AXPlatformNodeAuraLinux::GetHypertext() {
  return hypertext_;
}

const base::OffsetAdjuster::Adjustments&
AXPlatformNodeAuraLinux::GetHypertextAdjustments() {
  if (text_unicode_adjustments_.has_value())
    return *text_unicode_adjustments_;

  text_unicode_adjustments_.emplace();

  base::string16 text = GetText();
  int32_t text_length = text.size();
  for (int32_t i = 0; i < text_length; i++) {
    uint32_t code_point;
    size_t original_i = i;
    base::ReadUnicodeCharacter(text.c_str(), text_length + 1, &i, &code_point);

    if ((i - original_i + 1) != 1) {
      text_unicode_adjustments_->push_back(
          base::OffsetAdjuster::Adjustment(original_i, i - original_i + 1, 1));
    }
  }

  return *text_unicode_adjustments_;
}

size_t AXPlatformNodeAuraLinux::UTF16ToUnicodeOffsetInText(
    size_t utf16_offset) {
  size_t unicode_offset = utf16_offset;
  base::OffsetAdjuster::AdjustOffset(GetHypertextAdjustments(),
                                     &unicode_offset);
  return unicode_offset;
}

size_t AXPlatformNodeAuraLinux::UnicodeToUTF16OffsetInText(
    size_t unicode_offset) {
  size_t utf16_offset = unicode_offset;
  base::OffsetAdjuster::UnadjustOffset(GetHypertextAdjustments(),
                                       &utf16_offset);
  return utf16_offset;
}

int AXPlatformNodeAuraLinux::GetIndexInParent() {
  if (!GetParent())
    return -1;

  return delegate_->GetIndexInParent();
}

void AXPlatformNodeAuraLinux::SetExtentsRelativeToAtkCoordinateType(
    gint* x, gint* y, gint* width, gint* height, AtkCoordType coord_type) {
  gfx::Rect extents = delegate_->GetBoundsRect(AXCoordinateSystem::kScreen,
                                               AXClippingBehavior::kUnclipped);

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
  gfx::Rect rect_size = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
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
  action_data.action = ax::mojom::Action::kFocus;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::DoDefaultAction() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  return delegate_->AccessibilityPerformAction(action_data);
}

const gchar* AXPlatformNodeAuraLinux::GetDefaultActionName() {
  int action;
  if (!GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb, &action))
    return nullptr;

  base::string16 action_verb = ActionVerbToUnlocalizedString(
      static_cast<ax::mojom::DefaultActionVerb>(action));

  ATK_AURALINUX_RETURN_STRING(base::UTF16ToUTF8(action_verb));
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetAtkAttributes() {
  AtkAttributeSet* attribute_list = nullptr;
  ComputeAttributes(&attribute_list);
  return attribute_list;
}

AtkStateType AXPlatformNodeAuraLinux::GetAtkStateTypeForCheckableNode() {
  if (GetData().GetCheckedState() == ax::mojom::CheckedState::kMixed)
    return ATK_STATE_INDETERMINATE;
  if (GetData().role == ax::mojom::Role::kToggleButton)
    return ATK_STATE_PRESSED;
  return ATK_STATE_CHECKED;
}

// AtkDocumentHelpers

const gchar* AXPlatformNodeAuraLinux::GetDocumentAttributeValue(
    const gchar* attribute) const {
  if (!g_ascii_strcasecmp(attribute, "DocType"))
    return delegate_->GetTreeData().doctype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "MimeType"))
    return delegate_->GetTreeData().mimetype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "Title"))
    return delegate_->GetTreeData().title.c_str();
  else if (!g_ascii_strcasecmp(attribute, "URI"))
    return delegate_->GetTreeData().url.c_str();

  return nullptr;
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetDocumentAttributes() const {
  AtkAttributeSet* attribute_set = nullptr;
  const gchar* doc_attributes[] = {"DocType", "MimeType", "Title", "URI"};
  const gchar* value = nullptr;

  for (unsigned i = 0; i < G_N_ELEMENTS(doc_attributes); i++) {
    value = GetDocumentAttributeValue(doc_attributes[i]);
    if (value) {
      attribute_set = PrependAtkAttributeToAtkAttributeSet(
          doc_attributes[i], value, attribute_set);
    }
  }

  return attribute_set;
}

//
// AtkHyperlink helpers
//

AtkHyperlink* AXPlatformNodeAuraLinux::GetAtkHyperlink() {
  if (!atk_hyperlink_) {
    atk_hyperlink_ =
        ATK_HYPERLINK(g_object_new(AX_PLATFORM_ATK_HYPERLINK_TYPE, 0));
    ax_platform_atk_hyperlink_set_object(
        AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), this);
  }

  return atk_hyperlink_;
}

//
// Misc helpers
//

void AXPlatformNodeAuraLinux::GetFloatAttributeInGValue(
    ax::mojom::FloatAttribute attr,
    GValue* value) {
  float float_val;
  if (GetFloatAttribute(attr, &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

void AXPlatformNodeAuraLinux::AddAttributeToList(const char* name,
                                                 const char* value,
                                                 AtkAttributeSet** attributes) {
  *attributes = PrependAtkAttributeToAtkAttributeSet(name, value, *attributes);
}

void AXPlatformNodeAuraLinux::SetEmbeddedDocument(
    AtkObject* new_embedded_document) {
  SetWeakGPtrToAtkObject(&embedded_document_, new_embedded_document);
}

void AXPlatformNodeAuraLinux::SetEmbeddingWindow(
    AtkObject* new_embedding_window) {
  SetWeakGPtrToAtkObject(&embedding_window_, new_embedding_window);
}

base::string16 AXPlatformNodeAuraLinux::GetText() const {
  // Special case allows us to get text even in non-HTML case, e.g. browser UI.
  if (IsPlainTextField())
    return GetString16Attribute(ax::mojom::StringAttribute::kValue);

  if (IsChildOfLeaf())
    return AXPlatformNodeBase::GetText();

  return hypertext_.hypertext;
}

int AXPlatformNodeAuraLinux::GetCaretOffset() {
  if (!HasCaret())
    return -1;

  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  return UTF16ToUnicodeOffsetInText(selection_end);
}

bool AXPlatformNodeAuraLinux::SetCaretOffset(int offset) {
  int character_count = atk_text_get_character_count(ATK_TEXT(atk_object_));
  if (offset < 0 || offset > character_count)
    offset = character_count;

  offset = UnicodeToUTF16OffsetInText(offset);
  if (!SetTextSelection(offset, offset))
    return false;

  OnTextSelectionChanged();
  return true;
}

bool AXPlatformNodeAuraLinux::SetTextSelectionForAtkText(int start_offset,
                                                         int end_offset) {
  start_offset = UnicodeToUTF16OffsetInText(start_offset);
  end_offset = UnicodeToUTF16OffsetInText(end_offset);

  base::string16 text = GetText();
  if (start_offset < 0 || start_offset > static_cast<int>(text.length()))
    return false;
  if (end_offset < 0 || end_offset > static_cast<int>(text.length()))
    return false;

  bool result = SetTextSelection(start_offset, end_offset);
  if (result)
    OnTextSelectionChanged();
  return result;
}

bool AXPlatformNodeAuraLinux::HasSelection() {
  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  return selection_start >= 0 && selection_end >= 0 &&
         selection_start != selection_end;
}

// Since this method doesn't return a static gchar*, we expect the caller of
// atk_text_get_selection to free the return value.
gchar* AXPlatformNodeAuraLinux::GetSelection(int* start_offset,
                                             int* end_offset) {
  if (start_offset)
    *start_offset = 0;
  if (end_offset)
    *end_offset = 0;

  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  if (selection_start < 0 || selection_end < 0 ||
      selection_start == selection_end)
    return nullptr;

  // We should ignore the direction of the selection when exposing start and
  // end offsets. According to the ATK documentation the end offset is always
  // the offset immediately past the end of the selection. This wouldn't make
  // sense if end < start.
  if (selection_end < selection_start)
    std::swap(selection_start, selection_end);

  selection_start = UTF16ToUnicodeOffsetInText(selection_start);
  selection_end = UTF16ToUnicodeOffsetInText(selection_end);

  if (start_offset)
    *start_offset = selection_start;
  if (end_offset)
    *end_offset = selection_end;

  return atk_text::GetText(ATK_TEXT(atk_object_), selection_start,
                           selection_end);
}

bool AXPlatformNodeAuraLinux::IsInLiveRegion() {
  return GetData().HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus);
}

}  // namespace ui
