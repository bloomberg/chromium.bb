// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_MATCH_STATUS_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_MATCH_STATUS_CHECKER_H_

#include <string>

#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace fake_server {

// A matcher that checks a generic condition against the fake server. This class
// is abstract where any subclass will be responsible for implementing some of
// StatusChangeChecker's virtual methods.
class FakeServerMatchStatusChecker : public StatusChangeChecker,
                                     public FakeServer::Observer {
 public:
  FakeServerMatchStatusChecker();
  ~FakeServerMatchStatusChecker() override;

  // FakeServer::Observer implementation.
  void OnCommit(const std::string& committer_id,
                syncer::ModelTypeSet committed_model_types) override;

 protected:
  FakeServer* fake_server() const;

 private:
  FakeServer* fake_server_;
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_MATCH_STATUS_CHECKER_H_
