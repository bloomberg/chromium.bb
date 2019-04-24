// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

#include <stddef.h>

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"

#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"

namespace leveldb_proto {

namespace {

std::string PtotoDbTypeToString(ProtoDbType db_type) {
  switch (db_type) {
    case ProtoDbType::FEATURE_ENGAGEMENT_EVENT:
      return "FEATURE_ENGAGEMENT_EVENT";
    case ProtoDbType::FEATURE_ENGAGEMENT_AVAILABILITY:
      return "FEATURE_ENGAGEMENT_AVAILABILITY";
    case ProtoDbType::USAGE_STATS_WEBSITE_EVENT:
      return "USAGE_STATS_WEBSITE_EVENT";
    case ProtoDbType::USAGE_STATS_SUSPENSION:
      return "USAGE_STATS_SUSPENSION";
    case ProtoDbType::USAGE_STATS_TOKEN_MAPPING:
      return "USAGE_STATS_TOKEN_MAPPING";
    case ProtoDbType::LAST:
      NOTREACHED();
      break;
    case ProtoDbType::TEST_DATABASE0:
      return "TEST_DATABASE0";
    case ProtoDbType::TEST_DATABASE1:
      return "TEST_DATABASE1";
    case ProtoDbType::TEST_DATABASE2:
      return "TEST_DATABASE2";
  }
  return std::string();
}

constexpr ProtoDbType kWhitelistedListForSharedImpl[]{
    ProtoDbType::LAST,  // Marks the end of list.
};

const char* const kDBNameParamPrefix = "migrate_";

}  // namespace

// static
bool SharedProtoDatabaseClientList::ShouldUseSharedDB(ProtoDbType db_type) {
  for (size_t i = 0; kWhitelistedListForSharedImpl[i] != ProtoDbType::LAST;
       ++i) {
    if (kWhitelistedListForSharedImpl[i] == db_type)
      return true;
  }

  if (!base::FeatureList::IsEnabled(kProtoDBSharedMigration))
    return false;

  std::map<std::string, std::string> params;
  if (!base::GetFieldTrialParamsByFeature(kProtoDBSharedMigration, &params))
    return false;

  std::string name = PtotoDbTypeToString(db_type);
  return base::GetFieldTrialParamByFeatureAsBool(
      kProtoDBSharedMigration, kDBNameParamPrefix + name, false);
}

}  // namespace leveldb_proto