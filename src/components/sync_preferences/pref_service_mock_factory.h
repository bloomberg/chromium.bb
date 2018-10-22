// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_MOCK_FACTORY_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_MOCK_FACTORY_H_

#include "base/macros.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"

namespace sync_preferences {

// A helper that allows convenient building of custom PrefServices in tests.
class PrefServiceMockFactory : public PrefServiceSyncableFactory {
 public:
  PrefServiceMockFactory();
  ~PrefServiceMockFactory() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefServiceMockFactory);
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_MOCK_FACTORY_H_
