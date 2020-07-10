// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/extension_policy_migrator.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"

namespace policy {

namespace {

void DoNothing(base::Value* val) {}

}  // namespace

ExtensionPolicyMigrator::Migration::Migration(Migration&&) = default;

ExtensionPolicyMigrator::Migration::Migration(const char* old_name_,
                                              const char* new_name_)
    : Migration(old_name_, new_name_, base::BindRepeating(&DoNothing)) {}

ExtensionPolicyMigrator::Migration::Migration(const char* old_name_,
                                              const char* new_name_,
                                              ValueTransform transform_)
    : old_name(old_name_),
      new_name(new_name_),
      transform(std::move(transform_)) {}

ExtensionPolicyMigrator::Migration::~Migration() = default;

}  // namespace policy
