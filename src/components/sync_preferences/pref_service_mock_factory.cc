// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_mock_factory.h"

#include "components/prefs/testing_pref_store.h"

namespace sync_preferences {

PrefServiceMockFactory::PrefServiceMockFactory() {
  user_prefs_ = new TestingPrefStore;
}

PrefServiceMockFactory::~PrefServiceMockFactory() {}

}  // namespace sync_preferences
