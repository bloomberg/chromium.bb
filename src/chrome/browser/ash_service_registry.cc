// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash_service_registry.h"

#include "ash/ash_service.h"
#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "ash/components/tap_visualizer/public/mojom/tap_visualizer.mojom.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/accessibility/ax_host_service.h"
#include "chrome/browser/chromeos/prefs/pref_connector_service.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/service_manager_connection.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

using content::ContentBrowserClient;

namespace ash_service_registry {
namespace {

struct Service {
  const char* name;
  int display_name_id;  // Resource ID for the display name.
};

// Services shared between mash and non-mash configs.
constexpr Service kCommonServices[] = {
    {shortcut_viewer::mojom::kServiceName, IDS_ASH_SHORTCUT_VIEWER_APP_NAME},
    {tap_visualizer::mojom::kServiceName, IDS_ASH_TAP_VISUALIZER_APP_NAME},
};

// Services unique to mash. Note that the non-mash case also has an Ash service,
// it's just handled differently (see HandleServiceRequest()).
constexpr Service kMashServices[] = {
    {ash::mojom::kServiceName, IDS_ASH_ASH_SERVICE_NAME},
    {ws::mojom::kServiceName, IDS_ASH_UI_SERVICE_NAME},
};

void RegisterOutOfProcessServicesImpl(
    const Service* services,
    size_t num_services,
    ContentBrowserClient::OutOfProcessServiceMap* services_map) {
  for (size_t i = 0; i < num_services; ++i) {
    const Service& service = services[i];
    (*services_map)[service.name] =
        ContentBrowserClient::OutOfProcessServiceInfo(base::BindRepeating(
            &l10n_util::GetStringUTF16, service.display_name_id));
  }
}

}  // namespace

void RegisterOutOfProcessServices(
    ContentBrowserClient::OutOfProcessServiceMap* services) {
  RegisterOutOfProcessServicesImpl(kCommonServices, base::size(kCommonServices),
                                   services);
  if (features::IsMultiProcessMash()) {
    RegisterOutOfProcessServicesImpl(kMashServices, base::size(kMashServices),
                                     services);
  }
}

std::unique_ptr<service_manager::Service> HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (!features::IsMultiProcessMash() &&
      service_name == ash::mojom::kServiceName) {
    return std::make_unique<ash::AshService>(std::move(request));
  }
  if (service_name == ash::mojom::kPrefConnectorServiceName)
    return std::make_unique<AshPrefConnector>(std::move(request));
  if (service_name == ax::mojom::kAXHostServiceName)
    return std::make_unique<AXHostService>(std::move(request));

  return nullptr;
}

bool IsAshRelatedServiceName(const std::string& name) {
  for (const Service& service : kMashServices) {
    if (name == service.name)
      return true;
  }
  for (const Service& service : kCommonServices) {
    if (name == service.name)
      return true;
  }
  return false;
}

bool ShouldTerminateOnServiceQuit(const std::string& name) {
  // Some services going down are treated as catastrophic failures, usually
  // because both the browser and the service cache data about each other's
  // state that is not rebuilt when the service restarts.
  return name == ws::mojom::kServiceName || name == ash::mojom::kServiceName;
}

}  // namespace ash_service_registry
