// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HANDLE_CONVERTER_H_
#define PPAPI_PROXY_HANDLE_CONVERTER_H_

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

class PPAPI_PROXY_EXPORT HandleConverter {
 public:
  HandleConverter();

  // Convert the native handles in |msg| to NaCl style.
  // In some cases (e.g., Windows), we need to re-write the contents of the
  // message; in those cases, |new_msg_ptr| will be set to the new message.
  // If |msg| is already in a good form for NaCl, |new_msg_ptr| is left NULL.
  // See the explanation in the body of the method.
  //
  // In either case, all the handles in |msg| are extracted into |handles| so
  // that they can be converted to NaClDesc handles.
  // See chrome/nacl/nacl_ipc_adapter.cc for where this gets used.
  bool ConvertNativeHandlesToPosix(const IPC::Message& msg,
                                   std::vector<SerializedHandle>* handles,
                                   scoped_ptr<IPC::Message>* new_msg_ptr);

  // This method informs HandleConverter that a sync message is being sent so
  // that it can associate reply messages with their type.
  //
  // Users of HandleConverter must call this when they send a synchronous
  // message, otherwise HandleConverter won't be able to convert handles in
  // replies.
  void RegisterSyncMessageForReply(const IPC::Message& msg);

 private:
  // When we send a synchronous message (from untrusted to trusted), we store
  // its type here, so that later we can associate the reply with its type
  // and potentially translate handles in the message.
  typedef std::map<int, uint32> PendingSyncMsgMap;
  PendingSyncMsgMap pending_sync_msgs_;

  DISALLOW_COPY_AND_ASSIGN(HandleConverter);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HANDLE_CONVERTER_H_
