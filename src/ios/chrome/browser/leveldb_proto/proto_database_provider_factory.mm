// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/leveldb_proto/proto_database_provider_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace leveldb_proto {

// static
ProtoDatabaseProviderFactory* ProtoDatabaseProviderFactory::GetInstance() {
  static base::NoDestructor<ProtoDatabaseProviderFactory> instance;
  return instance.get();
}

// static
ProtoDatabaseProvider* ProtoDatabaseProviderFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ProtoDatabaseProvider*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

ProtoDatabaseProviderFactory::ProtoDatabaseProviderFactory()
    : BrowserStateKeyedServiceFactory(
          "leveldb_proto::ProtoDatabaseProvider",
          BrowserStateDependencyManager::GetInstance()) {}

ProtoDatabaseProviderFactory::~ProtoDatabaseProviderFactory() = default;

std::unique_ptr<KeyedService>
ProtoDatabaseProviderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<ProtoDatabaseProvider>(context->GetStatePath());
}

}  // namespace leveldb_proto