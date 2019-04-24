// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_CONTENT_PROTO_DATABASE_PROVIDER_FACTORY_H_
#define COMPONENTS_LEVELDB_PROTO_CONTENT_PROTO_DATABASE_PROVIDER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;

// A factory for ProtoDatabaseProvider, a class that provides proto databases
// stored in the appropriate directory for the current profile.
class ProtoDatabaseProviderFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of ProtoDatabaseProviderFactory.
  static ProtoDatabaseProviderFactory* GetInstance();

  // Returns ProtoDatabaseProvider associated with |context|, so we can
  // instantiate ProtoDatabases that use the appropriate profile directory.
  static ProtoDatabaseProvider* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ProtoDatabaseProviderFactory>;

  ProtoDatabaseProviderFactory();
  ~ProtoDatabaseProviderFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseProviderFactory);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_CONTENT_PROTO_DATABASE_PROVIDER_FACTORY_H_
