// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/icon_entry.h"

namespace notifications {

IconEntry::IconEntry() = default;

IconEntry::IconEntry(IconEntry&& other) {
  uuid.swap(other.uuid);
  data.swap(other.data);
}

}  // namespace notifications
