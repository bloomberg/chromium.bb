// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/browser_download_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/download/browser_download_service.h"
#import "ios/web/public/download/download_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
BrowserDownloadService* BrowserDownloadServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<BrowserDownloadService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
BrowserDownloadServiceFactory* BrowserDownloadServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserDownloadServiceFactory> instance;
  return instance.get();
}

BrowserDownloadServiceFactory::BrowserDownloadServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BrowserDownloadService",
          BrowserStateDependencyManager::GetInstance()) {}

BrowserDownloadServiceFactory::~BrowserDownloadServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserDownloadServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  web::DownloadController* download_controller =
      web::DownloadController::FromBrowserState(browser_state);
  return std::make_unique<BrowserDownloadService>(download_controller);
}

bool BrowserDownloadServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}

web::BrowserState* BrowserDownloadServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  return GetBrowserStateOwnInstanceInIncognito(browser_state);
}
