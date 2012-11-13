// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_APPLY_CONTROL_DATA_UPDATES_H_
#define SYNC_ENGINE_APPLY_CONTROL_DATA_UPDATES_H_

namespace syncer {

class Cryptographer;

namespace syncable {
class Directory;
class WriteTransaction;
}

void ApplyControlDataUpdates(syncable::Directory* dir);
bool ApplyNigoriUpdates(syncable::WriteTransaction* trans,
                        Cryptographer* cryptographer);

}  // namespace syncer

#endif  // SYNC_ENGINE_APPLY_CONTROL_DATA_UPDATES_H_
