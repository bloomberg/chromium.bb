// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"

#include "ash/public/ash_interfaces.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {

// The factory callback that will be used to create RecommendAppsFetcher
// instances other than default RecommendAppsFetcherImpl.
// It can be set by SetFactoryCallbackForTesting().
RecommendAppsFetcher::FactoryCallback* g_factory_callback = nullptr;

}  // namespace

// static
std::unique_ptr<RecommendAppsFetcher> RecommendAppsFetcher::Create(
    RecommendAppsFetcherDelegate* delegate) {
  if (g_factory_callback)
    return g_factory_callback->Run(delegate);

  mojo::PendingRemote<ash::mojom::CrosDisplayConfigController> display_config;
  ash::BindCrosDisplayConfigController(
      display_config.InitWithNewPipeAndPassReceiver());
  return std::make_unique<RecommendAppsFetcherImpl>(
      delegate, std::move(display_config),
      content::BrowserContext::GetDefaultStoragePartition(
          ProfileManager::GetActiveUserProfile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

// static
void RecommendAppsFetcher::SetFactoryCallbackForTesting(
    FactoryCallback* callback) {
  DCHECK(!g_factory_callback || !callback);

  g_factory_callback = callback;
}

}  // namespace chromeos
