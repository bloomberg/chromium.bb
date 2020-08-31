// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/cookie_helper.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace cookie_helper {

namespace {

// Name of the GAIA cookie that is being observed to detect when available
// accounts have changed in the content-area.
const char kSigninCookieName[] = "SAPISID";

}  // namespace

void AddSigninCookie(Profile* profile) {
  DCHECK(profile);
  net::CanonicalCookie cookie(
      kSigninCookieName, std::string(), ".google.com", "/", base::Time(),
      base::Time(), base::Time(), /*secure=*/true, false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);

  network::mojom::CookieManager* cookie_manager =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetCookieManagerForBrowserProcess();
  DCHECK(cookie_manager);

  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      cookie, net::cookie_util::SimulatedCookieSource(cookie, "https"),
      net::CookieOptions(),
      base::BindLambdaForTesting(
          [&run_loop](net::CanonicalCookie::CookieInclusionStatus) {
            run_loop.Quit();
          }));
  run_loop.Run();
}

void DeleteSigninCookies(Profile* profile) {
  DCHECK(profile);
  network::mojom::CookieManager* cookie_manager =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetCookieManagerForBrowserProcess();
  DCHECK(cookie_manager);

  base::RunLoop run_loop;
  network::mojom::CookieDeletionFilterPtr filter =
      network::mojom::CookieDeletionFilter::New();
  filter->cookie_name = kSigninCookieName;

  cookie_manager->DeleteCookies(
      std::move(filter),
      base::BindLambdaForTesting([&run_loop](uint32_t) { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace cookie_helper
