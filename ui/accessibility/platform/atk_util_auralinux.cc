// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <glib-2.0/gmodule.h>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace {

typedef void (*GnomeAccessibilityModuleInitFunc)();

const char kAccessibilityEnabled[] = "ACCESSIBILITY_ENABLED";
const char kAtkBridgeModule[] = "atk-bridge";
const char kAtkBridgePath[] = "gtk-2.0/modules/libatk-bridge.so";
const char kAtkBridgeSymbolName[] = "gnome_accessibility_module_init";
const char kGtkModules[] = "GTK_MODULES";

// Returns a function pointer to be invoked on the main thread to init
// the gnome accessibility module if it's enabled (nullptr otherwise).
GnomeAccessibilityModuleInitFunc GetAccessibilityModuleInitFunc() {
  base::AssertBlockingAllowed();

  // Try to load libatk-bridge.so.
  base::FilePath atk_bridge_path(ATK_LIB_DIR);
  atk_bridge_path = atk_bridge_path.Append(kAtkBridgePath);
  GModule* bridge = g_module_open(atk_bridge_path.value().c_str(),
                                  static_cast<GModuleFlags>(0));
  if (!bridge) {
    VLOG(1) << "Unable to open module " << atk_bridge_path.value();
    return nullptr;
  }

  GnomeAccessibilityModuleInitFunc init_func = nullptr;

  if (!g_module_symbol(bridge, kAtkBridgeSymbolName, (gpointer*)&init_func)) {
    VLOG(1) << "Unable to get symbol pointer from " << atk_bridge_path.value();
    return nullptr;
  }

  DCHECK(init_func);
  return init_func;
}

void FinishAccessibilityInitOnMainThread(
    GnomeAccessibilityModuleInitFunc init_func) {
  if (!init_func) {
    VLOG(1) << "Will not enable ATK accessibility support.";
    return;
  }

  init_func();
}

bool PlatformShouldEnableAccessibility() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string gtk_modules;
  if (!env->GetVar(kGtkModules, &gtk_modules))
    return false;

  for (const std::string& module :
       base::SplitString(gtk_modules, ":", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    if (module == kAtkBridgeModule)
      return true;
  }
  return false;
}

bool ShouldEnableAccessibility() {
  char* enable_accessibility = getenv(kAccessibilityEnabled);
  if ((enable_accessibility && atoi(enable_accessibility) == 1) ||
      PlatformShouldEnableAccessibility())
    return true;
  return false;
}

}  // namespace

G_BEGIN_DECLS

//
// atk_util_auralinux AtkObject definition and implementation.
//

#define ATK_UTIL_AURALINUX_TYPE (atk_util_auralinux_get_type())
#define ATK_UTIL_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
     ATK_UTIL_AURALINUX_TYPE, \
     AtkUtilAuraLinux))
#define ATK_UTIL_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), \
                             ATK_UTIL_AURALINUX_TYPE, \
                             AtkUtilAuraLinuxClass))
#define IS_ATK_UTIL_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), ATK_UTIL_AURALINUX_TYPE))
#define IS_ATK_UTIL_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), ATK_UTIL_AURALINUX_TYPE))
#define ATK_UTIL_AURALINUX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
                               ATK_UTIL_AURALINUX_TYPE, \
                               AtkUtilAuraLinuxClass))

typedef struct _AtkUtilAuraLinux        AtkUtilAuraLinux;
typedef struct _AtkUtilAuraLinuxClass   AtkUtilAuraLinuxClass;

struct _AtkUtilAuraLinux
{
  AtkUtil parent;
};

struct _AtkUtilAuraLinuxClass
{
  AtkUtilClass parent_class;
};

GType atk_util_auralinux_get_type();

G_DEFINE_TYPE(AtkUtilAuraLinux, atk_util_auralinux, ATK_TYPE_UTIL);

static void atk_util_auralinux_init(AtkUtilAuraLinux *ax_util) {
}

static AtkObject* atk_util_auralinux_get_root() {
  ui::AXPlatformNode* application = ui::AXPlatformNodeAuraLinux::application();
  if (application) {
    return application->GetNativeViewAccessible();
  }
  return nullptr;
}

static G_CONST_RETURN gchar* atk_util_auralinux_get_toolkit_name(void) {
  return "Chromium";
}

static G_CONST_RETURN gchar* atk_util_auralinux_get_toolkit_version(void) {
  return "1.0";
}

static void atk_util_auralinux_class_init(AtkUtilAuraLinuxClass *klass) {
  AtkUtilClass *atk_class;
  gpointer data;

  data = g_type_class_peek(ATK_TYPE_UTIL);
  atk_class = ATK_UTIL_CLASS(data);

  atk_class->get_root = atk_util_auralinux_get_root;
  atk_class->get_toolkit_name = atk_util_auralinux_get_toolkit_name;
  atk_class->get_toolkit_version = atk_util_auralinux_get_toolkit_version;
}

G_END_DECLS

//
// AtkUtilAuraLinuxClass implementation.
//

namespace ui {

// static
AtkUtilAuraLinux* AtkUtilAuraLinux::GetInstance() {
  return base::Singleton<AtkUtilAuraLinux>::get();
}

void AtkUtilAuraLinux::InitializeAsync() {
  // Register our util class.
  g_type_class_unref(g_type_class_ref(ATK_UTIL_AURALINUX_TYPE));

  if (!ShouldEnableAccessibility())
    return;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&GetAccessibilityModuleInitFunc),
      base::Bind(&FinishAccessibilityInitOnMainThread));
}

}  // namespace ui
