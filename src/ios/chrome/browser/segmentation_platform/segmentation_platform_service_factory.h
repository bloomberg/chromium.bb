// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace segmentation_platform {

class SegmentationPlatformService;

// Factory for SegmentationPlatformService.
class SegmentationPlatformServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static SegmentationPlatformService* GetForBrowserState(
      ChromeBrowserState* context);

  static SegmentationPlatformServiceFactory* GetInstance();

  SegmentationPlatformServiceFactory(SegmentationPlatformServiceFactory&) =
      delete;
  SegmentationPlatformServiceFactory& operator=(
      SegmentationPlatformServiceFactory&) = delete;

  // Returns the default factory used to build SegmentationPlatformService. Can
  // be registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SegmentationPlatformServiceFactory>;

  SegmentationPlatformServiceFactory();
  ~SegmentationPlatformServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
