// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/url_validity_checker_factory.h"

#include <memory>

#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
UrlValidityChecker* UrlValidityCheckerFactory::GetUrlValidityChecker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<UrlValidityCheckerImpl> checker(
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::DefaultTickClock::GetInstance());
  return checker.get();
}

UrlValidityCheckerFactory::UrlValidityCheckerFactory() = default;

UrlValidityCheckerFactory::~UrlValidityCheckerFactory() = default;
