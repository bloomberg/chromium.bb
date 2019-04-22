// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_METRICS_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_METRICS_H_

#include <string>

namespace leveldb {
class Status;
}  // namespace leveldb

namespace leveldb_proto {

// Static metrics recording helper functions for ProtoLevelDBWrapper.
//
// When adding database clients that require UMA metrics recording, ensure that
// the client name is added as a suffix in histograms.xml for the appropriate
// ProtoDB.* metrics.
class ProtoLevelDBWrapperMetrics {
 public:
  static void RecordInit(const std::string& client,
                         const leveldb::Status& status);
  static void RecordUpdate(const std::string& client,
                           bool success,
                           const leveldb::Status& status);
  static void RecordGet(const std::string& client,
                        bool success,
                        bool found,
                        const leveldb::Status& status);
  static void RecordLoadKeys(const std::string& client, bool success);
  static void RecordLoadEntries(const std::string& client, bool success);
  static void RecordLoadKeysAndEntries(const std::string& client, bool success);
  static void RecordDestroy(const std::string& client, bool success);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_METRICS_H_