// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/test/fake_server/fake_server.h"

ServerDeviceInfoMatchChecker::ServerDeviceInfoMatchChecker(
    const Matcher& matcher)
    : matcher_(matcher) {}

ServerDeviceInfoMatchChecker::~ServerDeviceInfoMatchChecker() = default;

void ServerDeviceInfoMatchChecker::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  if (committed_model_types.Has(syncer::DEVICE_INFO)) {
    CheckExitCondition();
  }
}

bool ServerDeviceInfoMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}
