// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RECORD_CHANGED_CALLBACK_H_
#define DISCOVERY_MDNS_MDNS_RECORD_CHANGED_CALLBACK_H_

namespace openscreen {
namespace discovery {

class MdnsRecord;

enum class RecordChangedEvent {
  kCreated,
  kUpdated,
  kExpired,
};

class MdnsRecordChangedCallback {
 public:
  virtual ~MdnsRecordChangedCallback() = default;
  virtual void OnRecordChanged(const MdnsRecord& record,
                               RecordChangedEvent event) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_RECORD_CHANGED_CALLBACK_H_
