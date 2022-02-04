// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefRegistrySimple;

namespace segmentation_platform {
class ServiceProxy;
struct SegmentSelectionResult;

// The core class of segmentation platform that integrates all the required
// pieces on the client side.
class SegmentationPlatformService : public KeyedService,
                                    public base::SupportsUserData {
 public:
#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type SegmentationPlatformService for the given
  // SegmentationPlatformService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      SegmentationPlatformService* segmentation_platform_service);
#endif  // BUILDFLAG(IS_ANDROID)

  SegmentationPlatformService() = default;
  ~SegmentationPlatformService() override = default;

  // Disallow copy/assign.
  SegmentationPlatformService(const SegmentationPlatformService&) = delete;
  SegmentationPlatformService& operator=(const SegmentationPlatformService&) =
      delete;

  // Registers preferences used by this class in the provided |registry|.  This
  // should be called for the Profile registry.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  using SegmentSelectionCallback =
      base::OnceCallback<void(const SegmentSelectionResult&)>;

  // Called to get the selected segment asynchronously. If none, returns empty
  // result.
  virtual void GetSelectedSegment(const std::string& segmentation_key,
                                  SegmentSelectionCallback callback) = 0;

  // Called to get the selected segment synchronously. If none, returns empty
  // result.
  virtual SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) = 0;

  // Called to enable or disable metrics collection. Must be explicitly called
  // on startup.
  virtual void EnableMetrics(bool signal_collection_allowed) = 0;

  // Called to get the proxy that is used for debugging purpose.
  virtual ServiceProxy* GetServiceProxy();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
