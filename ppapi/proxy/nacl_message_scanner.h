// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_NACL_MESSAGE_SCANNER_H_
#define PPAPI_PROXY_NACL_MESSAGE_SCANNER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class SerializedHandle;

class PPAPI_PROXY_EXPORT NaClMessageScanner {
 public:
  NaClMessageScanner();

  // Scans the message for items that require special handling. Copies any
  // SerializedHandles in the message into |handles| and if the message must be
  // rewritten for NaCl, sets |new_msg_ptr| to the new message. If no handles
  // are found, |handles| is left unchanged. If no rewriting is needed,
  // |new_msg_ptr| is left unchanged.
  //
  // See more explanation in the method definition.
  //
  // See chrome/nacl/nacl_ipc_adapter.cc for where this is used to help convert
  // native handles to NaClDescs.
  bool ScanMessage(const IPC::Message& msg,
                   std::vector<SerializedHandle>* handles,
                   scoped_ptr<IPC::Message>* new_msg_ptr);

  // This method informs NaClMessageScanner that a sync message is being sent
  // so that it can associate reply messages with their type.
  //
  // Users of NaClMessageScanner must call this when they send a synchronous
  // message, otherwise NaClMessageScanner won't scan replies.
  void RegisterSyncMessageForReply(const IPC::Message& msg);

 private:
  // When we send a synchronous message (from untrusted to trusted), we store
  // its type here, so that later we can associate the reply with its type
  // for scanning.
  typedef std::map<int, uint32> PendingSyncMsgMap;
  PendingSyncMsgMap pending_sync_msgs_;

  DISALLOW_COPY_AND_ASSIGN(NaClMessageScanner);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_NACL_MESSAGE_SCANNER_H_
