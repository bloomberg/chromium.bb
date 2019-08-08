// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/proto_database_provider_test_base.h"

#include "components/leveldb_proto/content/proto_database_provider_factory.h"

namespace previews {

ProtoDatabaseProviderTestBase::ProtoDatabaseProviderTestBase() = default;
ProtoDatabaseProviderTestBase::~ProtoDatabaseProviderTestBase() = default;

void ProtoDatabaseProviderTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  // Create a SimpleFactoryKey, to be used to retrieve a ProtoDatabaseProvider
  // to initialize a HintCacheStore.
  simple_factory_key_ = std::make_unique<SimpleFactoryKey>(temp_dir_.GetPath());
  db_provider_ =
      leveldb_proto::ProtoDatabaseProviderFactory::GetInstance()->GetForKey(
          simple_factory_key_.get());
}

void ProtoDatabaseProviderTestBase::TearDown() {
  // When done we remove the key from the factory, as it will persist in the
  // next test and we don't want to retrieve a provider from a past test.
  leveldb_proto::ProtoDatabaseProviderFactory::GetInstance()
      ->RemoveKeyForTesting(simple_factory_key_.get());
  simple_factory_key_.reset();
}

}  // namespace previews
