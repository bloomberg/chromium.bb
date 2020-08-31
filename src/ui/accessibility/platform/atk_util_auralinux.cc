// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/environment.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace {

const char* kAccessibilityEnabledVariables[] = {
    "ACCESSIBILITY_ENABLED",
    "GNOME_ACCESSIBILITY",
    "QT_ACCESSIBILITY",
};

//
// AtkUtilAuraLinux definition and implementation.
//
struct AtkUtilAuraLinux {
  AtkUtil parent;
};

struct AtkUtilAuraLinuxClass {
  AtkUtilClass parent_class;
};

G_DEFINE_TYPE(AtkUtilAuraLinux, atk_util_auralinux, ATK_TYPE_UTIL)

static void atk_util_auralinux_init(AtkUtilAuraLinux *ax_util) {
}

static AtkObject* AtkUtilAuraLinuxGetRoot() {
  ui::AXPlatformNode* application = ui::AXPlatformNodeAuraLinux::application();
  if (application) {
    return application->GetNativeViewAccessible();
  }
  return nullptr;
}

using KeySnoopFuncMap = std::map<guint, std::pair<AtkKeySnoopFunc, gpointer>>;
static KeySnoopFuncMap& GetActiveKeySnoopFunctions() {
  static base::NoDestructor<KeySnoopFuncMap> active_key_snoop_functions;
  return *active_key_snoop_functions;
}

static guint AtkUtilAuraLinuxAddKeyEventListener(
    AtkKeySnoopFunc key_snoop_function,
    gpointer data) {
  if (!ui::AXPlatformNode::GetAccessibilityMode().has_mode(
          ui::AXMode::kNativeAPIs))
    return 0;

  static guint current_key_event_listener_id = 0;

  current_key_event_listener_id++;
  GetActiveKeySnoopFunctions()[current_key_event_listener_id] =
      std::make_pair(key_snoop_function, data);
  return current_key_event_listener_id;
}

static void AtkUtilAuraLinuxRemoveKeyEventListener(guint listener_id) {
  GetActiveKeySnoopFunctions().erase(listener_id);
}

static void atk_util_auralinux_class_init(AtkUtilAuraLinuxClass *klass) {
  AtkUtilClass* atk_class = ATK_UTIL_CLASS(g_type_class_peek(ATK_TYPE_UTIL));

  atk_class->get_root = AtkUtilAuraLinuxGetRoot;
  atk_class->get_toolkit_name = []() { return "Chromium"; };
  atk_class->get_toolkit_version = []() { return "1.0"; };
  atk_class->add_key_event_listener = AtkUtilAuraLinuxAddKeyEventListener;
  atk_class->remove_key_event_listener = AtkUtilAuraLinuxRemoveKeyEventListener;
}

}  // namespace

namespace ui {

// static
AtkUtilAuraLinux* AtkUtilAuraLinux::GetInstance() {
  return base::Singleton<AtkUtilAuraLinux>::get();
}

bool AtkUtilAuraLinux::ShouldEnableAccessibility() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  for (const auto* variable : kAccessibilityEnabledVariables) {
    std::string enable_accessibility;
    env->GetVar(variable, &enable_accessibility);
    if (enable_accessibility == "1")
      return true;
  }

  return false;
}

void AtkUtilAuraLinux::InitializeAsync() {
  static bool initialized = false;

  if (initialized || !ShouldEnableAccessibility())
    return;

  initialized = true;

  // Register our util class.
  g_type_class_unref(g_type_class_ref(atk_util_auralinux_get_type()));

  PlatformInitializeAsync();
}

void AtkUtilAuraLinux::InitializeForTesting() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(kAccessibilityEnabledVariables[0], "1");

  InitializeAsync();
}

// static
// Disable CFI-icall since the key snooping function could be in another DSO.
__attribute__((no_sanitize("cfi-icall")))
DiscardAtkKeyEvent AtkUtilAuraLinux::HandleAtkKeyEvent(
    AtkKeyEventStruct* key_event) {
  DCHECK(key_event);

  if (!GetInstance()->ShouldEnableAccessibility())
    return DiscardAtkKeyEvent::Retain;

  GetInstance()->InitializeAsync();

  bool discard = false;
  for (auto& entry : GetActiveKeySnoopFunctions()) {
    AtkKeySnoopFunc key_snoop_function = entry.second.first;
    gpointer data = entry.second.second;

    // We want to ensure that all functions are called. We will discard this
    // event if at least one function suggests that we do it, but we still
    // need to call the functions that follow it in the map iterator.
    if (key_snoop_function(key_event, data) != 0)
      discard = true;
  }
  return discard ? DiscardAtkKeyEvent::Discard : DiscardAtkKeyEvent::Retain;
}

}  // namespace ui
