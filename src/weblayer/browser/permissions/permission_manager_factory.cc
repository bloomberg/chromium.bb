// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/permission_manager_factory.h"

#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/geolocation_permission_context_delegate.h"

#if defined(OS_ANDROID)
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#else
#include "components/permissions/contexts/geolocation_permission_context.h"
#endif

namespace weblayer {
namespace {

// Permission context which denies all requests.
class DeniedPermissionContext : public permissions::PermissionContextBase {
 public:
  using PermissionContextBase::PermissionContextBase;

 protected:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override {
    return CONTENT_SETTING_BLOCK;
  }

  bool IsRestrictedToSecureOrigins() const override { return true; }
};

// A permission context with default behavior, which is restricted to secure
// origins.
class SafePermissionContext : public permissions::PermissionContextBase {
 public:
  using PermissionContextBase::PermissionContextBase;
  SafePermissionContext(const SafePermissionContext&) = delete;
  SafePermissionContext& operator=(const SafePermissionContext&) = delete;

 protected:
  bool IsRestrictedToSecureOrigins() const override { return true; }
};

permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    content::BrowserContext* browser_context) {
  permissions::PermissionManager::PermissionContextMap permission_contexts;
#if defined(OS_ANDROID)
  using GeolocationPermissionContext =
      permissions::GeolocationPermissionContextAndroid;
#else
  using GeolocationPermissionContext =
      permissions::GeolocationPermissionContext;
#endif
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<GeolocationPermissionContext>(
          browser_context,
          std::make_unique<GeolocationPermissionContextDelegate>());

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<SafePermissionContext>(
          browser_context, ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
          blink::mojom::FeaturePolicyFeature::kEncryptedMedia);
#endif

  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<SafePermissionContext>(
          browser_context, ContentSettingsType::MEDIASTREAM_MIC,
          blink::mojom::FeaturePolicyFeature::kMicrophone);
  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<SafePermissionContext>(
          browser_context, ContentSettingsType::MEDIASTREAM_CAMERA,
          blink::mojom::FeaturePolicyFeature::kCamera);

  // For now, all requests are denied. As features are added, their permission
  // contexts can be added here instead of DeniedPermissionContext.
  for (content::PermissionType type : content::GetAllPermissionTypes()) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // PROTECTED_MEDIA_IDENTIFIER is only supported on Android/ChromeOS.
    if (type == content::PermissionType::PROTECTED_MEDIA_IDENTIFIER)
      continue;
#endif
    ContentSettingsType content_settings_type =
        permissions::PermissionManager::PermissionTypeToContentSetting(type);
    if (permission_contexts.find(content_settings_type) ==
        permission_contexts.end()) {
      permission_contexts[content_settings_type] =
          std::make_unique<DeniedPermissionContext>(
              browser_context, content_settings_type,
              blink::mojom::FeaturePolicyFeature::kNotFound);
    }
  }
  return permission_contexts;
}
}  // namespace

// static
permissions::PermissionManager* PermissionManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<permissions::PermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PermissionManagerFactory* PermissionManagerFactory::GetInstance() {
  static base::NoDestructor<PermissionManagerFactory> factory;
  return factory.get();
}

PermissionManagerFactory::PermissionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionManagerFactory::~PermissionManagerFactory() = default;

KeyedService* PermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::PermissionManager(context,
                                            CreatePermissionContexts(context));
}

content::BrowserContext* PermissionManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
