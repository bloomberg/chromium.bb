// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/subscriber_crosapi.h"

#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace {

bool Accepts(apps::mojom::AppType app_type) {
  return app_type == apps::mojom::AppType::kUnknown ||
         app_type == apps::mojom::AppType::kArc ||
         app_type == apps::mojom::AppType::kWeb ||
         app_type == apps::mojom::AppType::kSystemWeb ||
         app_type == apps::mojom::AppType::kStandaloneBrowserChromeApp;
}

bool Accepts(const std::vector<apps::mojom::AppPtr>& deltas) {
  for (const auto& delta : deltas) {
    if (!Accepts(delta->app_type)) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace apps {

SubscriberCrosapi::SubscriberCrosapi(Profile* profile) : profile_(profile) {}

SubscriberCrosapi::~SubscriberCrosapi() = default;

void SubscriberCrosapi::RegisterAppServiceProxyFromCrosapi(
    mojo::PendingReceiver<crosapi::mojom::AppServiceProxy> receiver) {
  // At the moment the app service subscriber will only accept one client
  // connect to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (crosapi_receiver_.is_bound()) {
    return;
  }
  crosapi_receiver_.Bind(std::move(receiver));
  crosapi_receiver_.set_disconnect_handler(base::BindOnce(
      &SubscriberCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void SubscriberCrosapi::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                               apps::mojom::AppType app_type,
                               bool should_notify_initialized) {
  if (Accepts(app_type) && Accepts(deltas) && subscriber_.is_bound()) {
    subscriber_->OnApps(std::move(deltas), app_type, should_notify_initialized);
  }
}

void SubscriberCrosapi::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SubscriberCrosapi::OnPreferredAppsChanged(
    apps::mojom::PreferredAppChangesPtr changes) {
  if (!subscriber_.is_bound()) {
    return;
  }
  subscriber_->OnPreferredAppsChanged(std::move(changes));
}

void SubscriberCrosapi::InitializePreferredApps(
    PreferredAppsList::PreferredApps preferred_apps) {
  if (!subscriber_.is_bound()) {
    return;
  }
  subscriber_->InitializePreferredApps(std::move(preferred_apps));
}

void SubscriberCrosapi::OnCrosapiDisconnected() {
  crosapi_receiver_.reset();
  subscriber_.reset();
}

void SubscriberCrosapi::RegisterAppServiceSubscriber(
    mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber) {
  // At the moment the app service subscriber will only accept one client
  // connect to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (subscriber_.is_bound()) {
    return;
  }
  subscriber_.Bind(std::move(subscriber));
  subscriber_.set_disconnect_handler(base::BindOnce(
      &SubscriberCrosapi::OnSubscriberDisconnected, base::Unretained(this)));

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  DCHECK(app_service.is_bound());
  mojo::PendingRemote<apps::mojom::Subscriber> app_service_subscriber;
  receivers_.Add(this, app_service_subscriber.InitWithNewPipeAndPassReceiver());
  app_service->RegisterSubscriber(std::move(app_service_subscriber), nullptr);
}

void SubscriberCrosapi::Launch(crosapi::mojom::LaunchParamsPtr launch_params) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  // TODO(crbug.com/1244506): Link up the return callback.
  proxy->LaunchAppWithParams(
      ConvertCrosapiToLaunchParams(launch_params, profile_), base::DoNothing());
}

void SubscriberCrosapi::LoadIcon(const std::string& app_id,
                                 apps::mojom::IconKeyPtr mojom_icon_key,
                                 IconType icon_type,
                                 int32_t size_hint_in_dip,
                                 apps::LoadIconCallback callback) {
  if (!mojom_icon_key) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  apps::mojom::AppType app_type = proxy->AppRegistryCache().GetAppType(app_id);
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    std::unique_ptr<IconKey> icon_key =
        ConvertMojomIconKeyToIconKey(mojom_icon_key);
    proxy->LoadIconFromIconKey(ConvertMojomAppTypToAppType(app_type), app_id,
                               *icon_key, icon_type, size_hint_in_dip,
                               /*allow_placeholder_icon=*/false,
                               std::move(callback));
  } else {
    proxy->LoadIconFromIconKey(
        app_type, app_id, std::move(mojom_icon_key),
        ConvertIconTypeToMojomIconType(icon_type), size_hint_in_dip,
        /*allow_placeholder_icon=*/false,
        MojomIconValueToIconValueCallback(std::move(callback)));
  }
}

void SubscriberCrosapi::AddPreferredApp(const std::string& app_id,
                                        crosapi::mojom::IntentPtr intent) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->AddPreferredApp(
      app_id, apps_util::ConvertCrosapiToAppServiceIntent(intent, profile_));
}

void SubscriberCrosapi::OnSubscriberDisconnected() {
  subscriber_.reset();
}

}  // namespace apps
