// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_PROTO_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_PROTO_UTILS_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/password_manager/core/browser/password_form.h"

namespace sync_pb {
class PasswordSpecifics;
class PasswordSpecificsData;
class PasswordSpecificsData_PasswordIssues;
class PasswordWithLocalData;
class ListPasswordsResult;
}  // namespace sync_pb

namespace password_manager {

// Converts a map of `form_password_issues` into the format required by the
// proto.
sync_pb::PasswordSpecificsData_PasswordIssues PasswordIssuesMapToProto(
    const base::flat_map<InsecureType, InsecurityMetadata>&
        form_password_issues);

// Builds a map of password issues from the proto data. The map is required
// for storing issues in a `PasswordForm`.
base::flat_map<InsecureType, InsecurityMetadata> PasswordIssuesMapFromProto(
    const sync_pb::PasswordSpecificsData& password_data);

// Returns sync_pb::PasswordSpecifics based on given `password_form`.
sync_pb::PasswordSpecifics SpecificsFromPassword(
    const PasswordForm& password_form);

// Returns sync_pb::PasswordSpecificsData based on given `password_form`.
sync_pb::PasswordSpecificsData SpecificsDataFromPassword(
    const PasswordForm& password_form);

// Returns sync_pb::PasswordWithLocalData based on given `password_form`.
sync_pb::PasswordWithLocalData PasswordWithLocalDataFromPassword(
    const PasswordForm& password_form);

// Returns a partial PasswordForm for a given set of `password_data`. In
// contrast to `PasswordFromProtoWithLocalData`, this method resets local data.
PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password_data);

// Returns a PasswordForm for a given `password` with local, chrome-specific
// data.
PasswordForm PasswordFromProtoWithLocalData(
    const sync_pb::PasswordWithLocalData& password);

// Converts the `list_result` to PasswordForms and returns them in a vector.
std::vector<PasswordForm> PasswordVectorFromListResult(
    const sync_pb::ListPasswordsResult& list_result);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_PROTO_UTILS_H_
