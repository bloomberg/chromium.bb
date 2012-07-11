// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_BLOB_H_
#define SYNC_SYNCABLE_BLOB_H_

#include <vector>

#include "base/basictypes.h"  // For uint8.

namespace syncer {
namespace syncable {

typedef std::vector<uint8> Blob;

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_BLOB_H_
