// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/chrome_xr_integration_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/build_config.h"
#include "content/public/browser/xr_install_helper.h"
#include "content/public/common/content_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"
#elif BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/vr/gvr_install_helper.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/android/vr/ar_jni_headers/ArCompositorDelegateProviderImpl_jni.h"
#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "components/webxr/android/arcore_device_provider.h"
#include "components/webxr/android/arcore_install_helper.h"
#endif  // BUILDFLAG(ENABLE_ARCORE)
#endif  // BUILDFLAG(IS_WIN)

namespace vr {

std::unique_ptr<content::XrInstallHelper>
ChromeXrIntegrationClient::GetInstallHelper(
    device::mojom::XRDeviceId device_id) {
  switch (device_id) {
#if BUILDFLAG(IS_ANDROID)
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
      return std::make_unique<GvrInstallHelper>();
#if BUILDFLAG(ENABLE_ARCORE)
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
      return std::make_unique<webxr::ArCoreInstallHelper>();
#endif  // BUILDFLAG(ENABLE_ARCORE)
#endif  // BUILDFLAG(IS_ANDROID)
    default:
      return nullptr;
  }
}

content::XRProviderList ChromeXrIntegrationClient::GetAdditionalProviders() {
  content::XRProviderList providers;

#if BUILDFLAG(IS_ANDROID)
  providers.push_back(std::make_unique<device::GvrDeviceProvider>());
#if BUILDFLAG(ENABLE_ARCORE)
  // TODO(https://crbug.com/966647) remove this check.
  if (base::FeatureList::IsEnabled(features::kWebXrArModule)) {
    base::android::ScopedJavaLocalRef<jobject>
        j_ar_compositor_delegate_provider =
            vr::Java_ArCompositorDelegateProviderImpl_Constructor(
                base::android::AttachCurrentThread());

    providers.push_back(std::make_unique<webxr::ArCoreDeviceProvider>(
        webxr::ArCompositorDelegateProvider(
            std::move(j_ar_compositor_delegate_provider))));
  }
#endif  // BUILDFLAG(ENABLE_ARCORE)
#endif  // BUILDFLAG(IS_ANDROID)

  return providers;
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<content::VrUiHost> ChromeXrIntegrationClient::CreateVrUiHost(
    device::mojom::XRDeviceId device_id,
    mojo::PendingRemote<device::mojom::XRCompositorHost> compositor) {
  return std::make_unique<VRUiHostImpl>(device_id, std::move(compositor));
}
#endif
}  // namespace vr
