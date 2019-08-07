// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/file/user_id_map.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/token.h"

namespace file {

namespace {

using TokenToPathMap = std::map<base::Token, base::FilePath>;

TokenToPathMap& GetTokenToPathMap() {
  static base::NoDestructor<TokenToPathMap> map;
  return *map;
}

}  // namespace

void AssociateServiceInstanceGroupWithUserDir(const base::Token& instance_group,
                                              const base::FilePath& user_dir) {
  GetTokenToPathMap()[instance_group] = user_dir;
}

void ForgetServiceInstanceGroupUserDirAssociation(
    const base::Token& instance_group) {
  GetTokenToPathMap().erase(instance_group);
}

base::FilePath GetUserDirForInstanceGroup(const base::Token& instance_group) {
  auto& map = GetTokenToPathMap();
  auto it = map.find(instance_group);
  DCHECK(it != map.end());
  return it->second;
}

}  // namespace file
