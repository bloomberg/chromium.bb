// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/fake_safe_browsing_service.h"

#include "base/bind_helpers.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#import "ios/chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A SafeBrowsingUrlCheckerImpl that treats all URLs as safe, unless they have
// host safe.browsing.unsafe.chromium.test.
class FakeSafeBrowsingUrlCheckerImpl
    : public safe_browsing::SafeBrowsingUrlCheckerImpl {
 public:
  explicit FakeSafeBrowsingUrlCheckerImpl(
      safe_browsing::ResourceType resource_type)
      : SafeBrowsingUrlCheckerImpl(
            resource_type,
            base::MakeRefCounted<UrlCheckerDelegateImpl>(
                /*database_manager=*/nullptr),
            base::Bind([]() { return static_cast<web::WebState*>(nullptr); })) {
  }
  ~FakeSafeBrowsingUrlCheckerImpl() override = default;

  // SafeBrowsingUrlCheckerImpl:
  void CheckUrl(
      const GURL& url,
      const std::string& method,
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback
          callback) override {
    if (url.host() == FakeSafeBrowsingService::kUnsafeHost) {
      std::move(callback).Run(/*slow_check_notifier=*/nullptr,
                              /*proceed=*/false,
                              /*showed_interstitial=*/true);
      return;
    }
    std::move(callback).Run(/*slow_check_notifier=*/nullptr, /*proceed=*/true,
                            /*showed_interstitial=*/false);
  }
};
}

// static
const std::string FakeSafeBrowsingService::kUnsafeHost =
    "safe.browsing.unsafe.chromium.test";

FakeSafeBrowsingService::FakeSafeBrowsingService() = default;

FakeSafeBrowsingService::~FakeSafeBrowsingService() = default;

void FakeSafeBrowsingService::Initialize(PrefService* prefs,
                                         const base::FilePath& user_data_path) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

void FakeSafeBrowsingService::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
FakeSafeBrowsingService::CreateUrlChecker(
    safe_browsing::ResourceType resource_type,
    web::WebState* web_state) {
  return std::make_unique<FakeSafeBrowsingUrlCheckerImpl>(resource_type);
}

bool FakeSafeBrowsingService::CanCheckUrl(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIsWSOrWSS();
}
