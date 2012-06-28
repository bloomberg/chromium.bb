// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNCAPI_INTERNAL_H_
#define SYNC_INTERNAL_API_SYNCAPI_INTERNAL_H_

// The functions defined are shared among some of the classes that implement
// the internal sync API.  They are not to be used by clients of the API.

#include <string>

namespace sync_pb {
class EntitySpecifics;
class PasswordSpecificsData;
}

namespace syncer {

class Cryptographer;

sync_pb::PasswordSpecificsData* DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics,
    syncer::Cryptographer* crypto);

void SyncAPINameToServerName(const std::string& syncer_name,
                             std::string* out);

bool IsNameServerIllegalAfterTrimming(const std::string& name);

bool AreSpecificsEqual(const syncer::Cryptographer* cryptographer,
                       const sync_pb::EntitySpecifics& left,
                       const sync_pb::EntitySpecifics& right);
}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNCAPI_INTERNAL_H_
