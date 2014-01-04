// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_H_
#define SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/notifier/sync_system_resources.h"

namespace syncer {

// GCMNetworkChannel is an implementation of SyncNetworkChannel that routes
// messages through GCMProfileService.
class SYNC_EXPORT_PRIVATE GCMNetworkChannel
    : public SyncNetworkChannel {
 public:
  explicit GCMNetworkChannel();

  virtual ~GCMNetworkChannel();

  // SyncNetworkChannel implementation.
  virtual void SendEncodedMessage(const std::string& encoded_message) OVERRIDE;
  virtual void UpdateCredentials(const std::string& email,
      const std::string& token) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMNetworkChannel);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_H_
