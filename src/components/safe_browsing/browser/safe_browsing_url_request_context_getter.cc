// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/safe_browsing_url_request_context_getter.h"

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/cookie_store.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;

namespace safe_browsing {

SafeBrowsingURLRequestContextGetter::SafeBrowsingURLRequestContextGetter(
    scoped_refptr<net::URLRequestContextGetter> system_context_getter,
    const base::FilePath& user_data_dir)
    : shut_down_(false),
      user_data_dir_(user_data_dir),
      system_context_getter_(system_context_getter),
      network_task_runner_(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})) {
  DCHECK(!user_data_dir.empty());
  DCHECK(system_context_getter_);
}

net::URLRequestContext*
SafeBrowsingURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check if the service has been shut down.
  if (shut_down_)
    return nullptr;

  if (!safe_browsing_request_context_) {
    safe_browsing_request_context_.reset(new net::URLRequestContext());
    safe_browsing_request_context_->CopyFrom(
        system_context_getter_->GetURLRequestContext());
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), net::GetCookieStoreBackgroundSequencePriority(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

    // Set up the CookieStore
    content::CookieStoreConfig cookie_config(CookieFilePath(), false, false,
                                             nullptr);
    cookie_config.background_task_runner = background_task_runner;
    safe_browsing_cookie_store_ =
        content::CreateCookieStore(cookie_config, nullptr /* netlog */);
    safe_browsing_request_context_->set_cookie_store(
        safe_browsing_cookie_store_.get());

    safe_browsing_request_context_->set_name("safe_browsing");
  }

  return safe_browsing_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SafeBrowsingURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void SafeBrowsingURLRequestContextGetter::ServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  shut_down_ = true;
  URLRequestContextGetter::NotifyContextShuttingDown();
  safe_browsing_request_context_.reset();
}

void SafeBrowsingURLRequestContextGetter::DisableQuicOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |http_network_session_| is only initialized once GetURLRequestContext() is
  // (lazily) called. With most consumers shifting to use
  // SafeBrowsingNetworkContext instead of this class directly, now on startup
  // GetURLRequestContext() might not have been called yet. So expliclity call
  // it to make sure http_network_session_ is initialized. Don't call it though
  // if shutdown has already started.
  if (!http_network_session_ && !shut_down_)
    GetURLRequestContext();

  if (http_network_session_)
    http_network_session_->DisableQuic();
}

base::FilePath SafeBrowsingURLRequestContextGetter::GetBaseFilename() {
  base::FilePath path(user_data_dir_);
  return path.Append(kSafeBrowsingBaseFilename);
}

base::FilePath SafeBrowsingURLRequestContextGetter::CookieFilePath() {
  return base::FilePath(GetBaseFilename().value() + kCookiesFile);
}

SafeBrowsingURLRequestContextGetter::~SafeBrowsingURLRequestContextGetter() {}

}  // namespace safe_browsing
