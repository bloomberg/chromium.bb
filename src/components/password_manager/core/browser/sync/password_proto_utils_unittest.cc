// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_proto_utils.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/protocol/list_passwords_result.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/password_with_local_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::ElementsAre;
using testing::Eq;

constexpr time_t kIssuesCreationTime = 1337;

sync_pb::PasswordSpecificsData_PasswordIssues CreateSpecificsDataIssues(
    const std::vector<InsecureType>& issue_types) {
  sync_pb::PasswordSpecificsData_PasswordIssues remote_issues;
  for (auto type : issue_types) {
    sync_pb::PasswordSpecificsData_PasswordIssues_PasswordIssue remote_issue;
    remote_issue.set_date_first_detection_microseconds(
        base::Time::FromTimeT(kIssuesCreationTime)
            .ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
    remote_issue.set_is_muted(false);
    switch (type) {
      case InsecureType::kLeaked:
        *remote_issues.mutable_leaked_password_issue() = remote_issue;
        break;
      case InsecureType::kPhished:
        *remote_issues.mutable_phished_password_issue() = remote_issue;
        break;
      case InsecureType::kWeak:
        *remote_issues.mutable_weak_password_issue() = remote_issue;
        break;
      case InsecureType::kReused:
        *remote_issues.mutable_reused_password_issue() = remote_issue;
        break;
    }
  }
  return remote_issues;
}

sync_pb::PasswordSpecificsData CreateSpecificsData(
    const std::string& origin,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm,
    const std::vector<InsecureType>& issue_types) {
  sync_pb::PasswordSpecificsData password_specifics;
  password_specifics.set_origin(origin);
  password_specifics.set_username_element(username_element);
  password_specifics.set_username_value(username_value);
  password_specifics.set_password_element(password_element);
  password_specifics.set_signon_realm(signon_realm);
  password_specifics.set_scheme(static_cast<int>(PasswordForm::Scheme::kHtml));
  password_specifics.set_action(GURL(origin).spec());
  password_specifics.set_password_value("D3f4ultP4$$w0rd");
  password_specifics.set_date_last_used(kIssuesCreationTime);
  password_specifics.set_date_created(kIssuesCreationTime);
  password_specifics.set_date_password_modified_windows_epoch_micros(
      kIssuesCreationTime);
  password_specifics.set_blacklisted(false);
  password_specifics.set_type(
      static_cast<int>(PasswordForm::Type::kFormSubmission));
  password_specifics.set_times_used(1);
  password_specifics.set_display_name("display_name");
  password_specifics.set_avatar_url(GURL(origin).spec());
  password_specifics.set_federation_url(std::string());
  *password_specifics.mutable_password_issues() =
      CreateSpecificsDataIssues(issue_types);
  return password_specifics;
}

}  // namespace

TEST(PasswordProtoUtilsTest, ConvertIssueProtoToMapAndBack) {
  sync_pb::PasswordSpecificsData specifics_data =
      CreateSpecificsData("http://www.origin.com/", "username_element",
                          "username_value", "password_element", "signon_realm",
                          {InsecureType::kLeaked, InsecureType::kPhished,
                           InsecureType::kReused, InsecureType::kWeak});

  EXPECT_THAT(
      PasswordIssuesMapToProto(PasswordIssuesMapFromProto(specifics_data))
          .SerializeAsString(),
      Eq(specifics_data.password_issues().SerializeAsString()));
}

TEST(PasswordProtoUtilsTest, ConvertSpecificsToFormAndBack) {
  sync_pb::PasswordSpecifics specifics;
  *specifics.mutable_client_only_encrypted_data() =
      CreateSpecificsData("http://www.origin.com/", "username_element",
                          "username_value", "password_element", "signon_realm",
                          /*issue_types=*/{});

  EXPECT_THAT(SpecificsFromPassword(
                  PasswordFromSpecifics(specifics.client_only_encrypted_data()))
                  .SerializeAsString(),
              Eq(specifics.SerializeAsString()));
}

TEST(PasswordProtoUtilsTest,
     ConvertPasswordWithLocalDataToFullPasswordFormAndBack) {
  sync_pb::PasswordWithLocalData password_data;
  *password_data.mutable_password_specifics_data() = CreateSpecificsData(
      "http://www.origin.com/", "username_element", "username_value",
      "password_element", "signon_realm", {InsecureType::kLeaked});

  // TODO(crbug.com/1229654): Add and test password_data.local_chrome_data().

  PasswordForm form = PasswordFromProtoWithLocalData(password_data);
  EXPECT_THAT(form.url, Eq(GURL("http://www.origin.com/")));
  EXPECT_THAT(form.username_element, Eq(u"username_element"));
  EXPECT_THAT(form.username_value, Eq(u"username_value"));
  EXPECT_THAT(form.password_element, Eq(u"password_element"));
  EXPECT_THAT(form.signon_realm, Eq("signon_realm"));

  sync_pb::PasswordWithLocalData password_data_converted_back =
      PasswordWithLocalDataFromPassword(form);
  EXPECT_EQ(password_data.SerializeAsString(),
            password_data_converted_back.SerializeAsString());
}

TEST(PasswordProtoUtilsTest, ConvertListResultToFormVector) {
  sync_pb::ListPasswordsResult list_result;
  sync_pb::PasswordWithLocalData password1;
  *password1.mutable_password_specifics_data() =
      CreateSpecificsData("http://1.origin.com/", "username_1", "username_1",
                          "password_1", "signon_1", {InsecureType::kLeaked});
  sync_pb::PasswordWithLocalData password2;
  *password2.mutable_password_specifics_data() =
      CreateSpecificsData("http://2.origin.com/", "username_2", "username_2",
                          "password_2", "signon_2", {InsecureType::kLeaked});
  *list_result.add_password_data() = password1;
  *list_result.add_password_data() = password2;

  std::vector<PasswordForm> forms = PasswordVectorFromListResult(list_result);

  EXPECT_THAT(forms, ElementsAre(PasswordFromProtoWithLocalData(password1),
                                 PasswordFromProtoWithLocalData(password2)));
}

}  // namespace password_manager
