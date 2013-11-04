// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/nacl_message_scanner.h"

#include <vector>
#include "base/bind.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/proxy/serialized_var.h"

class NaClDescImcShm;

namespace IPC {
class Message;
}

namespace {

typedef std::vector<ppapi::proxy::SerializedHandle> Handles;

struct ScanningResults {
  ScanningResults() : handle_index(0) {}

  // Vector to hold handles found in the message.
  Handles handles;
  // Current handle index in the rewritten message. During the scan, it will be
  // be less than or equal to handles.size(). After the scan it should be equal.
  int handle_index;
  // The rewritten message. This may be NULL, so all ScanParam overloads should
  // check for NULL before writing to it. In some cases, a ScanParam overload
  // may set this to NULL when it can determine that there are no parameters
  // that need conversion. (See the ResourceMessageReplyParams overload.)
  scoped_ptr<IPC::Message> new_msg;
};

void WriteHandle(int handle_index,
                 const ppapi::proxy::SerializedHandle& handle,
                 IPC::Message* msg) {
  ppapi::proxy::SerializedHandle::WriteHeader(handle.header(), msg);

  // Now write the handle itself in POSIX style.
  msg->WriteBool(true);  // valid == true
  msg->WriteInt(handle_index);
}

// Define overloads for each kind of message parameter that requires special
// handling. See ScanTuple for how these get used.

// Overload to match SerializedHandle.
void ScanParam(const ppapi::proxy::SerializedHandle& handle,
               ScanningResults* results) {
  results->handles.push_back(handle);
  if (results->new_msg)
    WriteHandle(results->handle_index++, handle, results->new_msg.get());
}

void HandleWriter(int* handle_index,
                  IPC::Message* m,
                  const ppapi::proxy::SerializedHandle& handle) {
  WriteHandle((*handle_index)++, handle, m);
}

// Overload to match SerializedVar, which can contain handles.
void ScanParam(const ppapi::proxy::SerializedVar& var,
               ScanningResults* results) {
  std::vector<ppapi::proxy::SerializedHandle*> var_handles = var.GetHandles();
  // Copy any handles and then rewrite the message.
  for (size_t i = 0; i < var_handles.size(); ++i)
    results->handles.push_back(*var_handles[i]);
  if (results->new_msg)
    var.WriteDataToMessage(results->new_msg.get(),
                           base::Bind(&HandleWriter, &results->handle_index));
}

// For PpapiMsg_ResourceReply and the reply to PpapiHostMsg_ResourceSyncCall,
// the handles are carried inside the ResourceMessageReplyParams.
// NOTE: We only intercept handles from host->NaCl. The only kind of
//       ResourceMessageParams that travels this direction is
//       ResourceMessageReplyParams, so that's the only one we need to handle.
void ScanParam(const ppapi::proxy::ResourceMessageReplyParams& params,
               ScanningResults* results) {
  // If the resource reply params don't contain handles, NULL the new message
  // pointer to cancel further rewriting.
  // NOTE: This works because only handles currently need rewriting, and we
  //       know at this point that this message has none.
  if (params.handles().empty()) {
    results->new_msg.reset(NULL);
    return;
  }

  // If we need to rewrite the message, write everything before the handles
  // (there's nothing after the handles).
  if (results->new_msg) {
    params.WriteReplyHeader(results->new_msg.get());
    // IPC writes the vector length as an int before the contents of the
    // vector.
    results->new_msg->WriteInt(static_cast<int>(params.handles().size()));
  }
  for (Handles::const_iterator iter = params.handles().begin();
       iter != params.handles().end();
       ++iter) {
    // ScanParam will write each handle to the new message, if necessary.
    ScanParam(*iter, results);
  }
  // Tell ResourceMessageReplyParams that we have taken the handles, so it
  // shouldn't close them. The NaCl runtime will take ownership of them.
  params.ConsumeHandles();
}

// Overload to match all other types. If we need to rewrite the message,
// write the parameter.
template <class T>
void ScanParam(const T& param, ScanningResults* results) {
  if (results->new_msg)
    IPC::WriteParam(results->new_msg.get(), param);
}

// These just break apart the given tuple and run ScanParam over each param.
// The idea is to scan elements in the tuple which require special handling,
// and write them into the |results| struct.
template <class A>
void ScanTuple(const Tuple1<A>& t1, ScanningResults* results) {
  ScanParam(t1.a, results);
}
template <class A, class B>
void ScanTuple(const Tuple2<A, B>& t1, ScanningResults* results) {
  ScanParam(t1.a, results);
  ScanParam(t1.b, results);
}
template <class A, class B, class C>
void ScanTuple(const Tuple3<A, B, C>& t1, ScanningResults* results) {
  ScanParam(t1.a, results);
  ScanParam(t1.b, results);
  ScanParam(t1.c, results);
}
template <class A, class B, class C, class D>
void ScanTuple(const Tuple4<A, B, C, D>& t1, ScanningResults* results) {
  ScanParam(t1.a, results);
  ScanParam(t1.b, results);
  ScanParam(t1.c, results);
  ScanParam(t1.d, results);
}

template <class MessageType>
class MessageScannerImpl {
 public:
  explicit MessageScannerImpl(const IPC::Message* msg)
      : msg_(static_cast<const MessageType*>(msg)) {
  }
  bool ScanMessage(ScanningResults* results) {
    typename TupleTypes<typename MessageType::Schema::Param>::ValueTuple params;
    if (!MessageType::Read(msg_, &params))
      return false;
    ScanTuple(params, results);
    return true;
  }

  bool ScanReply(ScanningResults* results) {
    typename TupleTypes<typename MessageType::Schema::ReplyParam>::ValueTuple
        params;
    if (!MessageType::ReadReplyParam(msg_, &params))
      return false;
    // If we need to rewrite the message, write the message id first.
    if (results->new_msg) {
      results->new_msg->set_reply();
      int id = IPC::SyncMessage::GetMessageId(*msg_);
      results->new_msg->WriteInt(id);
    }
    ScanTuple(params, results);
    return true;
  }
  // TODO(dmichael): Add ScanSyncMessage for outgoing sync messages, if we ever
  //                 need to scan those.

 private:
  const MessageType* msg_;
};

}  // namespace

#define CASE_FOR_MESSAGE(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        MessageScannerImpl<MESSAGE_TYPE> scanner(&msg); \
        if (rewrite_msg) \
          results.new_msg.reset( \
              new IPC::Message(msg.routing_id(), msg.type(), \
                               IPC::Message::PRIORITY_NORMAL)); \
        if (!scanner.ScanMessage(&results)) \
          return false; \
        break; \
      }
#define CASE_FOR_REPLY(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        MessageScannerImpl<MESSAGE_TYPE> scanner(&msg); \
        if (rewrite_msg) \
          results.new_msg.reset( \
              new IPC::Message(msg.routing_id(), msg.type(), \
                               IPC::Message::PRIORITY_NORMAL)); \
        if (!scanner.ScanReply(&results)) \
          return false; \
        break; \
      }

namespace ppapi {
namespace proxy {

class SerializedHandle;

NaClMessageScanner::NaClMessageScanner() {
}

// Windows IPC differs from POSIX in that native handles are serialized in the
// message body, rather than passed in a separate FileDescriptorSet. Therefore,
// on Windows, any message containing handles must be rewritten in the POSIX
// format before we can send it to the NaCl plugin.
//
// On POSIX and Windows we have to rewrite PpapiMsg_CreateNaClChannel messages.
// These contain a handle with an invalid (place holder) descriptor. We need to
// locate this handle so it can be replaced with a valid one when the channel is
// created.
bool NaClMessageScanner::ScanMessage(
    const IPC::Message& msg,
    std::vector<SerializedHandle>* handles,
    scoped_ptr<IPC::Message>* new_msg_ptr) {
  DCHECK(handles);
  DCHECK(handles->empty());
  DCHECK(new_msg_ptr);
  DCHECK(!new_msg_ptr->get());

  bool rewrite_msg =
#if defined(OS_WIN)
      true;
#else
      (msg.type() == PpapiMsg_CreateNaClChannel::ID);
#endif


  // We can't always tell from the message ID if rewriting is needed. Therefore,
  // scan any message types that might contain a handle. If we later determine
  // that there are no handles, we can cancel the rewriting by clearing the
  // results.new_msg pointer.
  ScanningResults results;
  switch (msg.type()) {
    CASE_FOR_MESSAGE(PpapiMsg_CreateNaClChannel)
    CASE_FOR_MESSAGE(PpapiMsg_PPBAudio_NotifyAudioStreamCreated)
    CASE_FOR_MESSAGE(PpapiMsg_PPPMessaging_HandleMessage)
    CASE_FOR_MESSAGE(PpapiPluginMsg_ResourceReply)
    case IPC_REPLY_ID: {
      int id = IPC::SyncMessage::GetMessageId(msg);
      PendingSyncMsgMap::iterator iter(pending_sync_msgs_.find(id));
      if (iter == pending_sync_msgs_.end()) {
        NOTREACHED();
        return false;
      }
      uint32_t type = iter->second;
      pending_sync_msgs_.erase(iter);
      switch (type) {
        CASE_FOR_REPLY(PpapiHostMsg_PPBGraphics3D_GetTransferBuffer)
        CASE_FOR_REPLY(PpapiHostMsg_PPBImageData_CreateSimple)
        CASE_FOR_REPLY(PpapiHostMsg_ResourceSyncCall)
        CASE_FOR_REPLY(PpapiHostMsg_SharedMemory_CreateSharedMemory)
        default:
          // Do nothing for messages we don't know.
          break;
      }
      break;
    }
    default:
      // Do nothing for messages we don't know.
      break;
  }

  // Only messages containing handles need to be rewritten. If no handles are
  // found, don't return the rewritten message either. This must be changed if
  // we ever add new param types that also require rewriting.
  if (!results.handles.empty()) {
    handles->swap(results.handles);
    *new_msg_ptr = results.new_msg.Pass();
  }
  return true;
}

void NaClMessageScanner::RegisterSyncMessageForReply(const IPC::Message& msg) {
  DCHECK(msg.is_sync());

  int msg_id = IPC::SyncMessage::GetMessageId(msg);
  DCHECK(pending_sync_msgs_.find(msg_id) == pending_sync_msgs_.end());

  pending_sync_msgs_[msg_id] = msg.type();
}

}  // namespace proxy
}  // namespace ppapi
