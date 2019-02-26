// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/content/proto_database_provider_factory.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/proto_database_provider.h"
#include "content/public/browser/browser_context.h"

namespace leveldb_proto {

// static
ProtoDatabaseProviderFactory* ProtoDatabaseProviderFactory::GetInstance() {
  return base::Singleton<ProtoDatabaseProviderFactory>::get();
}

// static
leveldb_proto::ProtoDatabaseProvider*
ProtoDatabaseProviderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<leveldb_proto::ProtoDatabaseProvider*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ProtoDatabaseProviderFactory::ProtoDatabaseProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "leveldb_proto::ProtoDatabaseProvider",
          BrowserContextDependencyManager::GetInstance()) {}

ProtoDatabaseProviderFactory::~ProtoDatabaseProviderFactory() = default;

KeyedService* ProtoDatabaseProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  base::FilePath profile_dir = context->GetPath();
  return leveldb_proto::ProtoDatabaseProvider::Create(profile_dir);
}

}  // namespace leveldb_proto
