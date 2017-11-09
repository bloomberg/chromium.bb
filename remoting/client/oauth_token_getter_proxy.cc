// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/oauth_token_getter_proxy.h"

#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

// Runs entriely on the |task_runner| thread.
class OAuthTokenGetterProxy::Core {
 public:
  explicit Core(std::unique_ptr<OAuthTokenGetter> token_getter);
  ~Core();

  void RequestToken(
      const TokenCallback& on_access_token,
      scoped_refptr<base::SingleThreadTaskRunner> original_task_runner);

  void ResolveCallback(
      const TokenCallback& on_access_token,
      scoped_refptr<base::SingleThreadTaskRunner> original_task_runner,
      Status status,
      const std::string& user_email,
      const std::string& access_token);

  void InvalidateCache();

  base::WeakPtr<Core> GetWeakPtr();

 private:
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<OAuthTokenGetter> token_getter_;

  base::WeakPtr<Core> weak_ptr_;
  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

OAuthTokenGetterProxy::Core::Core(
    std::unique_ptr<OAuthTokenGetter> token_getter)
    : token_getter_(std::move(token_getter)), weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

OAuthTokenGetterProxy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void OAuthTokenGetterProxy::Core::RequestToken(
    const TokenCallback& on_access_token,
    scoped_refptr<base::SingleThreadTaskRunner> original_task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  token_getter_->CallWithToken(
      base::Bind(&OAuthTokenGetterProxy::Core::ResolveCallback, weak_ptr_,
                 on_access_token, original_task_runner));
}

void OAuthTokenGetterProxy::Core::ResolveCallback(
    const TokenCallback& on_access_token,
    scoped_refptr<base::SingleThreadTaskRunner> original_task_runner,
    Status status,
    const std::string& user_email,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (original_task_runner->BelongsToCurrentThread()) {
    on_access_token.Run(status, user_email, access_token);
    return;
  }

  original_task_runner->PostTask(
      FROM_HERE, base::Bind(on_access_token, status, user_email, access_token));
}

void OAuthTokenGetterProxy::Core::InvalidateCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  token_getter_->InvalidateCache();
}

base::WeakPtr<OAuthTokenGetterProxy::Core>
OAuthTokenGetterProxy::Core::GetWeakPtr() {
  return weak_ptr_;
}

// OAuthTokenGetterProxy

OAuthTokenGetterProxy::OAuthTokenGetterProxy(
    std::unique_ptr<OAuthTokenGetter> token_getter,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : core_(new Core(std::move(token_getter))), task_runner_(task_runner) {}

OAuthTokenGetterProxy::~OAuthTokenGetterProxy() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->DeleteSoon(FROM_HERE, core_.release());
  }
}

void OAuthTokenGetterProxy::CallWithToken(
    const OAuthTokenGetter::TokenCallback& on_access_token) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_to_reply =
      base::ThreadTaskRunnerHandle::Get();

  if (task_runner_->BelongsToCurrentThread()) {
    core_->RequestToken(on_access_token, task_runner_to_reply);
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OAuthTokenGetterProxy::Core::RequestToken,
                 core_->GetWeakPtr(), on_access_token, task_runner_to_reply));
}

void OAuthTokenGetterProxy::InvalidateCache() {
  if (task_runner_->BelongsToCurrentThread()) {
    core_->InvalidateCache();
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&OAuthTokenGetterProxy::Core::InvalidateCache,
                            core_->GetWeakPtr()));
}

}  // namespace remoting
