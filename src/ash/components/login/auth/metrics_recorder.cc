// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/metrics_recorder.h"

#include "ash/components/login/auth/auth_status_consumer.h"
#include "ash/components/login/auth/user_context.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"

namespace ash {
namespace {

// Histogram for tracking the reason of auth failure
constexpr char kFailureReasonHistogramName[] = "Login.FailureReason";

// Histogram for tracking the reason of login success
constexpr char kSuccessReasonHistogramName[] = "Login.SuccessReason";

// Histogram  prefix for tracking login flow
constexpr char kLoginFlowHistogramPrefix[] = "Login.Flow.";

// Limit definition of "many users"
constexpr int kManyUserLimit = 5;

// Suffix for grouping total user numbers. Should match suffixes of the
// Login.Flow.{HideUsers, ShowUsers}.* metrics in metadata/ash/histograms.xml
std::string ShowUserPrefix(bool show_users_on_signin) {
  return show_users_on_signin ? "ShowUsers." : "HideUsers.";
}

// Suffix for grouping user counts. Should match suffixes of the
// Login.Flow.{HideUsers, ShowUsers}.* metrics in metadata/ash/histograms.xml
std::string UserCountSuffix(int user_count) {
  DCHECK_GE(user_count, 0);
  if (user_count <= 0)
    return "0";

  if (user_count == 1)
    return "1";

  if (user_count == 2)
    return "2";

  if (user_count < kManyUserLimit)
    return "Few";

  return "Many";
}

}  // namespace

MetricsRecorder::MetricsRecorder() = default;

MetricsRecorder::~MetricsRecorder() = default;

void MetricsRecorder::OnAuthFailure(const AuthFailure::FailureReason& reason) {
  base::RecordAction(base::UserMetricsAction("Login_Failure"));
  UMA_HISTOGRAM_ENUMERATION(kFailureReasonHistogramName, reason,
                            AuthFailure::NUM_FAILURE_REASONS);
}

void MetricsRecorder::OnLoginSuccess(const SuccessReason& reason) {
  base::RecordAction(base::UserMetricsAction("Login_Success"));
  UMA_HISTOGRAM_ENUMERATION(kSuccessReasonHistogramName, reason,
                            SuccessReason::NUM_SUCCESS_REASONS);
}

void MetricsRecorder::OnGuestLoignSuccess() {
  base::RecordAction(base::UserMetricsAction("Login_GuestLoginSuccess"));
}

void MetricsRecorder::OnUserCount(bool user_count) {
  user_count_ = user_count;
  MaybeReportFlowMetrics();
}

void MetricsRecorder::OnShowUsersOnSignin(bool show_users_on_signin) {
  show_users_on_signin_ = show_users_on_signin;
  MaybeReportFlowMetrics();
}

void MetricsRecorder::OnEnableEphemeralUsers(bool enable_ephemeral_users) {
  enable_ephemeral_users_ = enable_ephemeral_users;
  MaybeUpdateUserLoginType();
}

void MetricsRecorder::OnIsUserNew(bool is_new_user) {
  is_new_user_ = is_new_user;
  MaybeUpdateUserLoginType();
}

void MetricsRecorder::OnIsLoginOffline(bool is_login_offline) {
  is_login_offline_ = is_login_offline;
  MaybeUpdateUserLoginType();
}

void MetricsRecorder::MaybeUpdateUserLoginType() {
  if (!is_login_offline_.has_value() || !is_new_user_.has_value() ||
      !enable_ephemeral_users_.has_value())
    return;

  if (is_login_offline_.value()) {
    user_login_type_ = MetricsRecorder::kOffline;
  } else if (!is_new_user_.value()) {
    // The rest 3 online login types are with either existing user and new users
    user_login_type_ = MetricsRecorder::kOnlineExisting;
  } else if (enable_ephemeral_users_.value()) {
    // The rest 2 new user login types are either ephemeral or new online users
    user_login_type_ = MetricsRecorder::kEphemeral;
  } else {
    user_login_type_ = MetricsRecorder::kOnlineNew;
  }

  MaybeReportFlowMetrics();
}

void MetricsRecorder::MaybeReportFlowMetrics() {
  if (!show_users_on_signin_.has_value() || !user_count_.has_value() ||
      !user_login_type_.has_value())
    return;

  std::string prefix =
      base::StrCat({kLoginFlowHistogramPrefix,
                    ShowUserPrefix(show_users_on_signin_.value())});
  std::string suffix = UserCountSuffix(user_count_.value());
  base::UmaHistogramEnumeration(base::StrCat({prefix, suffix}),
                                user_login_type_.value());
}

}  // namespace ash
