// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/handle_converter.h"

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_handle.h"

namespace IPC {
class Message;
}

namespace {

void WriteHandle(int handle_index,
                 const ppapi::proxy::SerializedHandle& handle,
                 IPC::Message* message) {
  ppapi::proxy::SerializedHandle::WriteHeader(handle.header(), message);

  // Now write the handle itself in POSIX style.
  message->WriteBool(true);  // valid == true
  message->WriteInt(handle_index);
}

typedef std::vector<ppapi::proxy::SerializedHandle> Handles;

// We define overload for catching SerializedHandles, so that we can share
// them correctly to the untrusted side, and another for handling all other
// parameters. See ConvertHandlesImpl for how these get used.
void ConvertHandlesInParam(const ppapi::proxy::SerializedHandle& handle,
                           Handles* handles,
                           IPC::Message* msg,
                           int* handle_index) {
  handles->push_back(handle);
  if (msg)
    WriteHandle((*handle_index)++, handle, msg);
}

// For PpapiMsg_ResourceReply and the reply to PpapiHostMsg_ResourceSyncCall,
// the handles are carried inside the ResourceMessageReplyParams.
// NOTE: We only translate handles from host->NaCl. The only kind of
//       ResourceMessageParams that travels this direction is
//       ResourceMessageReplyParams, so that's the only one we need to handle.
void ConvertHandlesInParam(
    const ppapi::proxy::ResourceMessageReplyParams& params,
    Handles* handles,
    IPC::Message* msg,
    int* handle_index) {
  // First, if we need to rewrite the message parameters, write everything
  // before the handles (there's nothing after the handles).
  if (msg) {
    params.WriteReplyHeader(msg);
    // IPC writes the vector length as an int before the contents of the
    // vector.
    msg->WriteInt(static_cast<int>(params.handles().size()));
  }
  for (Handles::const_iterator iter = params.handles().begin();
       iter != params.handles().end();
       ++iter) {
    // ConvertHandle will write each handle to |msg|, if necessary.
    ConvertHandlesInParam(*iter, handles, msg, handle_index);
  }
  // Tell ResourceMessageReplyParams that we have taken the handles, so it
  // shouldn't close them. The NaCl runtime will take ownership of them.
  params.ConsumeHandles();
}

// This overload is to catch all types other than SerializedHandle or
// ResourceMessageReplyParams. On Windows, |msg| will be a valid pointer, and we
// must write |param| to it.
template <class T>
void ConvertHandlesInParam(const T& param,
                           Handles* /* handles */,
                           IPC::Message* msg,
                           int* /* handle_index */) {
  // It's not a handle, so just write to the output message, if necessary.
  if (msg)
    IPC::WriteParam(msg, param);
}

// These just break apart the given tuple and run ConvertHandle over each param.
// The idea is to extract any handles in the tuple, while writing all data to
// msg (if msg is valid). The msg will only be valid on Windows, where we need
// to re-write all of the message parameters, writing the handles in POSIX style
// for NaCl.
template <class A>
void ConvertHandlesImpl(const Tuple1<A>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
}
template <class A, class B>
void ConvertHandlesImpl(const Tuple2<A, B>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.b, handles, msg, &handle_index);
}
template <class A, class B, class C>
void ConvertHandlesImpl(const Tuple3<A, B, C>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.b, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.c, handles, msg, &handle_index);
}
template <class A, class B, class C, class D>
void ConvertHandlesImpl(const Tuple4<A, B, C, D>& t1, Handles* handles,
                        IPC::Message* msg) {
  int handle_index = 0;
  ConvertHandlesInParam(t1.a, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.b, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.c, handles, msg, &handle_index);
  ConvertHandlesInParam(t1.d, handles, msg, &handle_index);
}

template <class MessageType>
class HandleConverterImpl {
 public:
  explicit HandleConverterImpl(const IPC::Message* msg)
      : msg_(static_cast<const MessageType*>(msg)) {
  }
  bool ConvertMessage(Handles* handles, IPC::Message* out_msg) {
    typename TupleTypes<typename MessageType::Schema::Param>::ValueTuple params;
    if (!MessageType::Read(msg_, &params))
      return false;
    ConvertHandlesImpl(params, handles, out_msg);
    return true;
  }

  bool ConvertReply(Handles* handles, IPC::SyncMessage* out_msg) {
    typename TupleTypes<typename MessageType::Schema::ReplyParam>::ValueTuple
        params;
    if (!MessageType::ReadReplyParam(msg_, &params))
      return false;
    // If we need to rewrite the message (i.e., on Windows), we need to make
    // sure we write the message id first.
    if (out_msg) {
      out_msg->set_reply();
      int id = IPC::SyncMessage::GetMessageId(*msg_);
      out_msg->WriteInt(id);
    }
    ConvertHandlesImpl(params, handles, out_msg);
    return true;
  }
  // TODO(dmichael): Add ConvertSyncMessage for outgoing sync messages, if we
  //                 ever pass handles in one of those.

 private:
  const MessageType* msg_;
};

}  // namespace

#define CASE_FOR_MESSAGE(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        HandleConverterImpl<MESSAGE_TYPE> extractor(&msg); \
        if (!extractor.ConvertMessage(handles, new_msg_ptr->get())) \
          return false; \
        break; \
      }
#define CASE_FOR_REPLY(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        HandleConverterImpl<MESSAGE_TYPE> extractor(&msg); \
        if (!extractor.ConvertReply( \
                handles, \
                static_cast<IPC::SyncMessage*>(new_msg_ptr->get()))) \
          return false; \
        break; \
      }

namespace ppapi {
namespace proxy {

class SerializedHandle;

HandleConverter::HandleConverter() {
}

bool HandleConverter::ConvertNativeHandlesToPosix(
    const IPC::Message& msg,
    std::vector<SerializedHandle>* handles,
    scoped_ptr<IPC::Message>* new_msg_ptr) {
  DCHECK(handles);
  DCHECK(new_msg_ptr);
  DCHECK(!new_msg_ptr->get());

  // In Windows, we need to re-write the contents of the message. This is
  // because in Windows IPC code, native HANDLE values are serialized in the
  // body of the message.
  //
  // In POSIX, we only serialize an index in to a FileDescriptorSet, and the
  // actual file descriptors are sent out-of-band. So on Windows, to make a
  // message that's compatible with Windows, we need to write a new message that
  // has simple indices in the message body instead of the HANDLEs.
  //
  // NOTE: This means on Windows, new_msg_ptr's serialized contents are not
  // compatible with Windows IPC deserialization code; it is intended to be
  // passed to NaCl.
#if defined(OS_WIN)
  new_msg_ptr->reset(
      new IPC::Message(msg.routing_id(), msg.type(), msg.priority()));
#else
  // Even on POSIX, we have to rewrite messages to create channels, because
  // these contain a handle with an invalid (place holder) descriptor. The
  // message sending code sees this and doesn't pass the descriptor over
  // correctly.
  if (msg.type() == PpapiMsg_CreateNaClChannel::ID) {
    new_msg_ptr->reset(
        new IPC::Message(msg.routing_id(), msg.type(), msg.priority()));
  }
#endif

  switch (msg.type()) {
    CASE_FOR_MESSAGE(PpapiMsg_CreateNaClChannel)
    CASE_FOR_MESSAGE(PpapiMsg_PPBAudio_NotifyAudioStreamCreated)
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
        CASE_FOR_REPLY(PpapiHostMsg_PPBImageData_CreateNaCl)
        CASE_FOR_REPLY(PpapiHostMsg_ResourceSyncCall)
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
  return true;
}

void HandleConverter::RegisterSyncMessageForReply(const IPC::Message& msg) {
  DCHECK(msg.is_sync());

  int msg_id = IPC::SyncMessage::GetMessageId(msg);
  DCHECK(pending_sync_msgs_.find(msg_id) == pending_sync_msgs_.end());

  pending_sync_msgs_[msg_id] = msg.type();
}

}  // namespace proxy
}  // namespace ppapi
