// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/db/allowlist_checker_client.h"

#include <memory>

#include "base/bind.h"
#include "components/safe_browsing/core/common/thread_utils.h"

namespace safe_browsing {

namespace {

// Number of milliseconds to wait for the response from the Safe Browsing
// database manager before proceeding with the timeout behavior.
const int kLookupTimeoutMS = 5000;
}  // namespace

// static
void AllowlistCheckerClient::StartCheckCsdWhitelist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::OnceCallback<void(bool)> callback_for_result) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // On timeout or if the list is unavailable, report match.
  const bool kDefaultDoesMatchAllowlist = true;

  std::unique_ptr<AllowlistCheckerClient> client = GetAllowlistCheckerClient(
      database_manager, url, &callback_for_result, kDefaultDoesMatchAllowlist);
  if (!client) {
    std::move(callback_for_result).Run(kDefaultDoesMatchAllowlist);
    return;
  }

  AsyncMatch match = database_manager->CheckCsdWhitelistUrl(url, client.get());
  InvokeCallbackOrRelease(match, std::move(client));
}

// static
void AllowlistCheckerClient::StartCheckHighConfidenceAllowlist(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::OnceCallback<void(bool)> callback_for_result) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // On timeout or if the list is unavailable, report no match.
  const bool kDefaultDoesMatchAllowlist = false;

  std::unique_ptr<AllowlistCheckerClient> client = GetAllowlistCheckerClient(
      database_manager, url, &callback_for_result, kDefaultDoesMatchAllowlist);
  if (!client) {
    std::move(callback_for_result).Run(kDefaultDoesMatchAllowlist);
    return;
  }

  AsyncMatch match =
      database_manager->CheckUrlForHighConfidenceAllowlist(url, client.get());
  InvokeCallbackOrRelease(match, std::move(client));
}

// static
std::unique_ptr<AllowlistCheckerClient>
AllowlistCheckerClient::GetAllowlistCheckerClient(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    const GURL& url,
    base::OnceCallback<void(bool)>* callback_for_result,
    bool default_does_match_allowlist) {
  if (!url.is_valid() || !database_manager ||
      !database_manager->CanCheckUrl(url)) {
    return nullptr;
  }

  // Make a client for each request. The caller could have several in
  // flight at once.
  return std::make_unique<AllowlistCheckerClient>(
      std::move(*callback_for_result), database_manager,
      default_does_match_allowlist);
}

// static
void AllowlistCheckerClient::InvokeCallbackOrRelease(
    AsyncMatch match,
    std::unique_ptr<AllowlistCheckerClient> client) {
  switch (match) {
    case AsyncMatch::MATCH:
      std::move(client->callback_for_result_)
          .Run(true /* did_match_allowlist */);
      break;
    case AsyncMatch::NO_MATCH:
      std::move(client->callback_for_result_)
          .Run(false /* did_match_allowlist */);
      break;
    case AsyncMatch::ASYNC:
      // Client is now self-owned. When it gets called back with the result,
      // it will delete itself.
      client.release();
      break;
  }
}

AllowlistCheckerClient::AllowlistCheckerClient(
    base::OnceCallback<void(bool)> callback_for_result,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool default_does_match_allowlist)
    : callback_for_result_(std::move(callback_for_result)),
      database_manager_(database_manager),
      default_does_match_allowlist_(default_does_match_allowlist) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // Set a timer to fail open, i.e. call it "whitelisted", if the full
  // check takes too long.
  auto timeout_callback = base::BindOnce(&AllowlistCheckerClient::OnTimeout,
                                         weak_factory_.GetWeakPtr());
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kLookupTimeoutMS),
               std::move(timeout_callback));
}

AllowlistCheckerClient::~AllowlistCheckerClient() {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
}

// SafeBrowsingDatabaseMananger::Client impl
void AllowlistCheckerClient::OnCheckWhitelistUrlResult(
    bool did_match_allowlist) {
  OnCheckUrlResult(did_match_allowlist);
}

void AllowlistCheckerClient::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  OnCheckUrlResult(did_match_allowlist);
}

void AllowlistCheckerClient::OnCheckUrlResult(bool did_match_allowlist) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  timer_.Stop();

  // The callback can only be invoked by other code paths if this object is not
  // self-owned. Because this method is only invoked when we're self-owned, we
  // know the callback must still be valid, and it must be safe to delete
  // |this|.
  DCHECK(callback_for_result_);
  std::move(callback_for_result_).Run(did_match_allowlist);
  delete this;
}

void AllowlistCheckerClient::OnTimeout() {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  database_manager_->CancelCheck(this);
  OnCheckUrlResult(default_does_match_allowlist_);
}

}  // namespace safe_browsing
