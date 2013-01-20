// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_LOCAL_SYNC_TEST_SERVER_H_
#define SYNC_TEST_LOCAL_SYNC_TEST_SERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/test/local_test_server.h"

namespace syncer {

// Runs a Python-based sync test server on the same machine on which the
// LocalSyncTestServer runs.
class LocalSyncTestServer : public net::LocalTestServer {
 public:
  // Initialize a sync server that listens on localhost using ephemeral ports
  // for sync and p2p notifications.
  LocalSyncTestServer();

  // Initialize a sync server that listens on |port| for sync updates and
  // |xmpp_port| for p2p notifications.
  LocalSyncTestServer(uint16 port, uint16 xmpp_port);

  virtual ~LocalSyncTestServer();

  // Overriden from net::LocalTestServer.
  virtual bool AddCommandLineArguments(
      CommandLine* command_line) const OVERRIDE;
  virtual bool SetPythonPath() const OVERRIDE;
  virtual bool GetTestServerPath(FilePath* testserver_path) const OVERRIDE;

  // Returns true if the path to |test_script_name| is successfully stored  in
  // |*test_script_path|. Used by the run_sync_testserver executable.
  bool GetTestScriptPath(const FilePath::StringType& test_script_name,
                         FilePath* test_script_path) const;

 private:
  // Port on which the Sync XMPP server listens.
  uint16 xmpp_port_;

  DISALLOW_COPY_AND_ASSIGN(LocalSyncTestServer);
};

}  // namespace syncer

#endif  // SYNC_TEST_LOCAL_SYNC_TEST_SERVER_H_
