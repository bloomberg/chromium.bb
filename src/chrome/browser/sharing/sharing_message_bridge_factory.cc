// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge_impl.h"

#include "base/memory/singleton.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace {
constexpr char kServiceName[] = "SharingMessageBridge";
}  // namespace

SharingMessageBridgeFactory::SharingMessageBridgeFactory()
    : BrowserContextKeyedServiceFactory(
          kServiceName,
          BrowserContextDependencyManager::GetInstance()) {}

SharingMessageBridgeFactory::~SharingMessageBridgeFactory() = default;

// static
SharingMessageBridgeFactory* SharingMessageBridgeFactory::GetInstance() {
  return base::Singleton<SharingMessageBridgeFactory>::get();
}

// static
SharingMessageBridge* SharingMessageBridgeFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SharingMessageBridge*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* SharingMessageBridgeFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SHARING_MESSAGE,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  return new SharingMessageBridgeImpl(std::move(change_processor));
}
