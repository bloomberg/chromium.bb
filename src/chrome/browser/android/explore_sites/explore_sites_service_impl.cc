// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"

namespace explore_sites {

ExploreSitesServiceImpl::ExploreSitesServiceImpl(
    std::unique_ptr<ExploreSitesStore> store)
    : explore_sites_store_(std::move(store)) {}

ExploreSitesServiceImpl::~ExploreSitesServiceImpl() {}

void ExploreSitesServiceImpl::Shutdown() {}

}  // namespace explore_sites
