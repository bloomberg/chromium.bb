// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SESSIONS_MOCK_DEBUG_INFO_GETTER_H_
#define SYNC_SESSIONS_MOCK_DEBUG_INFO_GETTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/debug_info_getter.h"

namespace syncer {
namespace sessions {

// A mock implementation of DebugInfoGetter to be used in tests. Events added by
// AddDebugEvent are accessible via DebugInfoGetter methods.
class MockDebugInfoGetter : public sessions::DebugInfoGetter {
 public:
  MockDebugInfoGetter();
  virtual ~MockDebugInfoGetter();

  // DebugInfoGetter implementation.
  virtual void GetDebugInfo(sync_pb::DebugInfo* debug_info) OVERRIDE;
  virtual void ClearDebugInfo() OVERRIDE;

  void AddDebugEvent();

 private:
  sync_pb::DebugInfo debug_info_;

  DISALLOW_COPY_AND_ASSIGN(MockDebugInfoGetter);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_MOCK_DEBUG_INFO_GETTER_H_
