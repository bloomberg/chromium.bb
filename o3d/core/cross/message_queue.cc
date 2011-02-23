/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the definition of the MessageQueue, the class that handles
// the communication of external code (clients) with O3D (server) via the
// NativeClient IMC library.

#if defined(OS_MACOSX) | defined(OS_LINUX)
#include <sys/types.h>
#include <unistd.h>
#endif
#include "core/cross/message_queue.h"
#include "core/cross/client_info.h"
#include "core/cross/object_manager.h"
#include "core/cross/bitmap.h"
#include "core/cross/texture.h"
#include "core/cross/error.h"
#include "core/cross/pointer_utils.h"
#include "core/cross/renderer.h"

#ifdef OS_WIN
#include "core/cross/core_metrics.h"
#endif

namespace o3d {

volatile ::base::subtle::Atomic32 MessageQueue::last_message_queue_id_ = -1;

// Prefix used to name all server socket addresses for O3D.
const char kServerSocketAddressPrefix[] = "o3d";

// Writes any nacl IMC errors to the log with a descriptive message.
// NOTE: macros used to make sure the LOG calls note the
// correct line number and source file.
#define LOG_IMC_ERROR(message) {                                \
  char buffer[256];                                             \
  if (nacl::GetLastErrorString(buffer, sizeof(buffer)) == 0) {  \
    LOG(ERROR) << message << " : " << buffer;                   \
  } else {                                                      \
    LOG(ERROR) << message;                                      \
  }                                                             \
}

ConnectedClient::ConnectedClient(nacl::Handle handle) : client_handle_(handle) {
}

ConnectedClient::~ConnectedClient() {
  std::vector<SharedMemoryInfo>::const_iterator iter;

  // Unmap and close shared memory.
  for (iter = shared_memory_array_.begin(); iter < shared_memory_array_.end();
       ++iter) {
    nacl::Unmap(iter->mapped_address, iter->size);
    nacl::Close(iter->shared_memory_handle);
  }
}

// Registers a newly created shared memory buffer with the
// ConnectedClient by adding the buffer info into the
// shared_memory_array_.
void ConnectedClient::RegisterSharedMemory(int32 buffer_id,
                                           nacl::Handle handle,
                                           void *address,
                                           int32 size) {
  SharedMemoryInfo shared_mem;
  shared_mem.buffer_id = buffer_id;
  shared_mem.shared_memory_handle = handle;
  shared_mem.mapped_address = address;
  shared_mem.size = size;

  shared_memory_array_.push_back(shared_mem);
}

// Unregisters a shared memory buffer for a given client-allocated
// memory region, unmapping and closing it in the process.
bool ConnectedClient::UnregisterSharedMemory(int32 buffer_id) {
  std::vector<SharedMemoryInfo>::iterator iter;
  for (iter = shared_memory_array_.begin(); iter < shared_memory_array_.end();
       ++iter) {
    if (iter->buffer_id == buffer_id) {
      nacl::Unmap(iter->mapped_address, iter->size);
      nacl::Close(iter->shared_memory_handle);
      shared_memory_array_.erase(iter);
      return true;
    }
  }
  return false;
}

// Returns the SharedMemoryInfo corresponding to the given shared
// memory buffer id.  The buffer must first be created by the
// MessageQueue on behalf of this ConnectedClient.
const SharedMemoryInfo* ConnectedClient::GetSharedMemoryInfo(int32 id) const {
  std::vector<SharedMemoryInfo>::const_iterator iter;
  for (iter = shared_memory_array_.begin(); iter < shared_memory_array_.end();
       ++iter) {
    if (iter->buffer_id == id) {
      return &(*iter);
    }
  }
  return NULL;
}


MessageQueue::MessageQueue(ServiceLocator* service_locator)
    : service_locator_(service_locator),
      object_manager_(service_locator->GetService<ObjectManager>()),
      server_socket_handle_(nacl::kInvalidHandle),
      next_shared_memory_id_(0) {
#if defined(OS_WIN)
  DWORD proc_id = GetCurrentProcessId();
#endif
#if defined(OS_MACOSX) | defined(OS_LINUX)
  pid_t proc_id = getpid();
#endif

  int id = ::base::subtle::NoBarrier_AtomicIncrement(&last_message_queue_id_,
                                                     1);

  // Create a unique name for the socket used by the message queue.
  // We use part of the process id to distinguish between different
  // browsers running o3d at the same time as well as a count to
  // distinguish between multiple instances of o3d running in the same
  // browser.
  ::base::snprintf(server_socket_address_.path,
                   sizeof(server_socket_address_.path),
                   "%s%u-%d",
                   kServerSocketAddressPrefix,
                   static_cast<unsigned int>(proc_id),
                   id);
}

MessageQueue::~MessageQueue() {
  // Clean up the ConnectedClient array.
  std::vector<ConnectedClient*>::const_iterator iter;
  for (iter = connected_clients_.begin(); iter != connected_clients_.end();
       ++iter) {
    nacl::Close((*iter)->client_handle());
    delete *iter;
  }

  // Close the socket.
  nacl::Close(server_socket_handle_);
}

// Creates a bound socket that corresponds to the communcation channel
// for this Client.
bool MessageQueue::Initialize() {
  server_socket_handle_ = nacl::BoundSocket(&server_socket_address_);

  if (server_socket_handle_ == nacl::kInvalidHandle) {
    LOG_IMC_ERROR("Failed to create a bound socket for the MessageQueue");
    return false;
  }

  return true;
}

String MessageQueue::GetSocketAddress() const {
  return &server_socket_address_.path[0];
}


// Checks the message queue for an incoming message.  If one is found
// then it processes it, otherwise it just returns.
bool MessageQueue::CheckForNewMessages(bool* has_new_texture) {
  // The flag will be set to true if we receive a new texture in
  // ProcessMessageUpdateTexture2DRect() or
  // ProcessMessageUpdateTexture2D()
  has_new_texture_ = false;

  // NOTE: This code uses reasonable defaults for the max
  // sizes of the received messages.  If a message uses more memory or
  // transmits more data handles then it will appear as truncated.  If
  // we find that there are valid messages with size larger than
  // what's defined here we should adjust the constants accordingly.
  const int kBufferLength = 1024;  // max 1K of memory transfered per message

  // max handles transfered per message
  const int kMaxNumHandles = nacl::kHandleCountMax;

  char message_buffer[kBufferLength];
  nacl::Handle handles[kMaxNumHandles];

  // All received messages are read as containing a single buffer for data
  // a number of handles.
  nacl::IOVec io_vec[1];
  io_vec[0].base = message_buffer;
  io_vec[0].length = kBufferLength;

  nacl::MessageHeader header;
  header.iov = io_vec;
  header.iov_length = 1;
  header.handles = handles;
  header.handle_count = kMaxNumHandles;
  header.flags = 0;

  // First check for a message in the server socket.  The only
  // messages that we should be receiving here are the HELLO messages
  // received from clients that want to connect.  Note that
  // ReceiveMessageFromSocket also returns true if there are no
  // messages in the queue in which case message_length will be equal
  // to -1.
  imc::MessageId message_id;
  int message_length = 0;
  if (ReceiveMessageFromSocket(server_socket_handle_,
                               &header,
                               &message_id,
                               &message_length)) {
    if (message_id == imc::HELLO) {
      ProcessHelloMessage(&header, handles);
#ifdef OS_WIN
      metric_imc_hello_msg.Set(true);
#endif
    } else if (message_length != -1) {
      DLOG(INFO) << "Received a non-HELLO message from server queue";
    }
  }

  // Check all the sockets of the connected clients to see if they contain any
  // messages.
  std::vector<ConnectedClient*>::iterator iter;
  for (iter = connected_clients_.begin(); iter < connected_clients_.end();) {
    // Must reset the available buffer length and number of handles each time
    // so NaCl's IMC knows how much space is available for reading
    io_vec[0].length = kBufferLength;
    header.handle_count = kMaxNumHandles;
    if (ReceiveMessageFromSocket((*iter)->client_handle(),
                                 &header,
                                 &message_id,
                                 &message_length)) {
      if (message_length == 0) {
        // Message length of 0 means EOF (i.e., client closed its handle).
        nacl::Close((*iter)->client_handle());
        delete *iter;
        iter = connected_clients_.erase(iter);  // Advances the iterator too.
        continue;
      }
      if (message_length != -1) {  // Else no message waiting
        ProcessClientRequest(*iter,
                             message_length,
                             message_id,
                             &header,
                             handles);
      }
    }
    ++iter;
  }

  *has_new_texture = has_new_texture_;
  return true;
}


// Checks the socket for messages.  If none are found then it returns
// right away.  If a message is found, it checks to make sure that the
// first 4 bytes contain a valid message ID.  If they do, then it
// return the ID in message_id.
bool MessageQueue::ReceiveMessageFromSocket(nacl::Handle socket,
                                            nacl::MessageHeader* header,
                                            imc::MessageId* message_id,
                                            int* length) {
  *message_id = imc::INVALID_ID;

  // Check if there's a new message but don't block waiting for it.
  int message_length = nacl::ReceiveDatagram(socket,
                                             header,
                                             nacl::kDontWait);

  // If result==-1 then either there are no messages in the queue in
  // which case we can just return, or the message read failed in
  // which case we need to log the failure.
  if (message_length == -1) {
    if (nacl::WouldBlock()) {
      *length = message_length;
      return true;
#if defined(OS_WIN)
    } else if (GetLastError() == ERROR_BROKEN_PIPE) {
      // On Windows, the NACL library treats EOF as a failure with this failure
      // code. We convert it to the traditional format of a successful read that
      // returns zero bytes to match the Mac & Linux case below.
      *length = 0;
      return true;
#endif
    } else {
      LOG_IMC_ERROR("nacl::ReceiveMessage failed");
      return false;
    }
  }

#if defined(OS_MACOSX) | defined(OS_LINUX)
  if (message_length == 0) {  // EOF
    *length = 0;
    return true;
  }
#endif

  // Valid messages must always contain at least the ID of the message
  if (message_length >= static_cast<int>(sizeof(*message_id))) {
    // Check if the incoming message requires more space than we have
    // currently allocated.
    if (header->flags & nacl::kMessageTruncated) {
      LOG(ERROR) << "Incoming message was truncated";
      return false;
    }

    // Extract the ID of the message just received.
    imc::MessageId id_found =
      *(reinterpret_cast<imc::MessageId*>(header->iov[0].base));
    if (id_found <= imc::INVALID_ID ||
        id_found >= imc::MAX_NUM_IDS) {
      LOG(ERROR) << "Unknown ID found in message :" << id_found;
    }
    *message_id = id_found;
    *length = message_length;
    return true;
  } else {
    LOG(ERROR) << "Incoming message too short (length:" << message_length
               << ")";
    return false;
  }
}

bool MessageQueue::ProcessClientRequest(ConnectedClient* client,
                                        int message_length,
                                        imc::MessageId message_id,
                                        nacl::MessageHeader* header,
                                        nacl::Handle* handles) {
  static int expected_message_lengths[] = {
    #define O3D_IMC_MESSAGE_OP(id, class_name)  sizeof(class_name::Msg),
    O3D_IMC_MESSAGE_LIST(O3D_IMC_MESSAGE_OP)
    #undef O3D_IMC_MESSAGE_OP
  };

  if (message_id == imc::INVALID_ID ||
      static_cast<unsigned>(message_id) >=
          arraysize(expected_message_lengths)) {
    LOG(ERROR) << "Unrecognized message id " << message_id;
    return false;
  }

  if (message_length != expected_message_lengths[message_id]) {
    LOG(ERROR) << "Bad message length for "
               << imc::GetMessageDescription(message_id);
    return false;
  }

  switch (message_id) {
    #define O3D_IMC_MESSAGE_OP(id, class_name) \
      case imc::id: return Process ## class_name( \
          client, message_length, header, handles, \
          *static_cast<const class_name::Msg*>(header->iov[0].base));
    O3D_IMC_MESSAGE_LIST(O3D_IMC_MESSAGE_OP)
    #undef O3D_IMC_MESSAGE_OP
    default:
      return false;
  }

  return true;
}

bool MessageQueue::SendBooleanResponse(nacl::Handle client_handle, bool value) {
  int response = (value ? 1 : 0);
  nacl::IOVec vec;
  vec.base = &response;
  vec.length = sizeof(response);

  nacl::MessageHeader header;
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  int result = nacl::SendDatagram(client_handle, &header, 0);

  if (result != sizeof(response)) {
    LOG_IMC_ERROR("Failed to send boolean response to client handle");
    return false;
  }

  return true;
}

// Processes a HELLO message received from a client.  If everything goes well
// it adds the client to the ConnectedClient list and sends back a positive
// response.
bool MessageQueue::ProcessHelloMessage(nacl::MessageHeader *header,
                                       nacl::Handle *handles) {
  // HELLO is the first message that should be send by a client.  It should
  // contain a single handle corresponding to the client's socket.
  if (header->handle_count == 1) {
    nacl::Handle client_handle = header->handles[0];
    // Make sure the handle is not already being used (i.e. only allow
    // a single HELLO message from a client)
    // TODO : please check correctness of this line
    std::vector<ConnectedClient*>::const_iterator find_iter = find(
        connected_clients_.begin(), connected_clients_.end(),
        reinterpret_cast<ConnectedClient*>(client_handle));
    if (find_iter != connected_clients_.end()) {
      LOG(WARNING) << "Received HELLO from client that's already connected";

      // Tell the client that the handshake failed.
      SendBooleanResponse(client_handle, false);
      return true;
    }

    // Send an acknowledgement back to the client that the handshake succeeded.
    if (!SendBooleanResponse(client_handle, true))
      return false;

    // TODO Is there any way to verify that the handle we got
    // passed here actually corresponds to the socket handle of the client?
    ConnectedClient* new_client = new ConnectedClient(client_handle);

    // Add the new client to the list.
    connected_clients_.push_back(new_client);
    return true;
  }
  return false;
}

// All emums need a Process function.
bool MessageQueue::ProcessMessageInvalidId(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageInvalidId::Msg& message) {
  return false;
}

bool MessageQueue::ProcessMessageHello(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageHello::Msg& message) {
  // Hello is handled special.
  return false;
}

// Processes a request to allocate a shared memory buffer on behalf of a
// connected client.  Parses the arguments of the message to determine how
// much space is requested, it creates the shared memory buffer, maps it in
// the local address space and sends a message back to the client with the
// newly created memory handle.
bool MessageQueue::ProcessMessageAllocateSharedMemory(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageAllocateSharedMemory::Msg& message) {

  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for ALLOCATE_SHARED_MEMORY";
    return false;
  }

  int32 mem_size = message.mem_size;
  if (mem_size <= 0 ||
      mem_size > MessageAllocateSharedMemory::Msg::kMaxSharedMemSize) {
    LOG(ERROR) << "Invalid mem size requested: " << mem_size
               << "(max size = "
               << MessageAllocateSharedMemory::Msg::kMaxSharedMemSize << ")";
    return false;
  }

  // Create the shared memory object.
  nacl::Handle shared_memory = nacl::CreateMemoryObject(mem_size);
  if (shared_memory == nacl::kInvalidHandle) {
    LOG_IMC_ERROR("Failed to create shared memory object");
    return false;
  }

  // Map it in local address space.
  void* shared_region = nacl::Map(0,
                                  mem_size,
                                  nacl::kProtRead | nacl::kProtWrite,
                                  nacl::kMapShared,
                                  shared_memory,
                                  0);

  if (shared_region == nacl::kMapFailed) {
    LOG_IMC_ERROR("Failed to map shared memory");
    nacl::Close(shared_memory);
    return false;
  }

  // Create a unique id for the shared memory buffer.
  MessageAllocateSharedMemory::Response response(next_shared_memory_id_++);

  // Send the shared memory handle and the buffer id back to the client.
  nacl::MessageHeader response_header;
  nacl::IOVec id_vec;
  id_vec.base = &response.data;
  id_vec.length = sizeof(response.data);

  response_header.iov = &id_vec;
  response_header.iov_length = 1;
  response_header.handles = &shared_memory;
  response_header.handle_count = 1;
  int result = nacl::SendDatagram(client->client_handle(), &response_header, 0);

  if (result != sizeof(response.data)) {
    LOG_IMC_ERROR("Failed to send shared memory handle back to the client");
    nacl::Unmap(shared_region, mem_size);
    nacl::Close(shared_memory);
    return false;
  }

  // Register the newly created shared memory with the connected client.
  client->RegisterSharedMemory(response.data.buffer_id,
                               shared_memory,
                               shared_region,
                               mem_size);

  return true;
}

// Processes a request by a client to update the contents of a Texture object
// bitmap using data stored in a shared memory region.  The client sends the
// id of the shared memory region, an offset in that region, the id of the
// Texture object, the level to be modified and the number of bytes to copy.
// This is essentially asynchronous as the client will not receive a response
// back from the server
bool MessageQueue::ProcessMessageUpdateTexture2D(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageUpdateTexture2D::Msg& message) {
  // Check the length of the message to make sure it contains the size of
  // the requested buffer.
  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for UPDATE_TEXTURE2D";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  // Check that this client did actually allocate the shared memory
  // corresponding to this handle.
  const SharedMemoryInfo* info =
      client->GetSharedMemoryInfo(message.shared_memory_id);
  if (info == NULL) {
    O3D_ERROR(service_locator_)
        << "shared memory id " << message.shared_memory_id << " not found";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  // Check that the Id passed in actually corresponds to a texture.
  Texture2D* texture_object =
      object_manager_->GetById<Texture2D>(message.texture_id);

  if (texture_object == NULL) {
    O3D_ERROR(service_locator_)
        << "Texture with id " << message.texture_id << " not found";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  // Check that we will not be reading past the end of the allocated shared
  // memory.
  if (message.offset + message.number_of_bytes > info->size ||
      message.offset + message.number_of_bytes < message.offset) {
    O3D_ERROR(service_locator_)
        << "Offset + texture size exceeds allocated shared memory size ("
        << message.offset << " + " << message.number_of_bytes << " > "
        << info->size;
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  unsigned int mip_width =
      image::ComputeMipDimension(message.level, texture_object->width());

  void *target_address =
      PointerFromVoidPointer<void*>(info->mapped_address, message.offset);

  int pitch = image::ComputePitch(texture_object->format(), mip_width);
  int rows = message.number_of_bytes / pitch;

  texture_object->SetRect(
      message.level, 0, 0, mip_width, rows, target_address, pitch);

  int remain = message.number_of_bytes % pitch;
  if (remain) {
    int width = remain / image::ComputePitch(texture_object->format(), 1);
    texture_object->SetRect(
        message.level, 0, rows, width, 1,
        AddPointerOffset<void*>(target_address, rows * pitch), pitch);
  }

  SendBooleanResponse(client->client_handle(), true);
  has_new_texture_ = true;
  return true;
}

bool MessageQueue::ProcessMessageUpdateTexture2DRect(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageUpdateTexture2DRect::Msg& message) {
  // Check the length of the message to make sure it contains the size of
  // the requested buffer.
  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for UPDATE_TEXTURE2D_RECT";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  // Check that this client did actually allocate the shared memory
  // corresponding to this handle.
  const SharedMemoryInfo* info =
      client->GetSharedMemoryInfo(message.shared_memory_id);
  if (info == NULL) {
    O3D_ERROR(service_locator_)
        << "shared memory id " << message.shared_memory_id << " not found";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  // Check that the Id passed in actually corresponds to a texture.
  Texture2D* texture_object =
      object_manager_->GetById<Texture2D>(message.texture_id);

  if (texture_object == NULL) {
    O3D_ERROR(service_locator_)
        << "Texture with id " << message.texture_id << " not found";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  // Check that we will not be reading past the end of the allocated shared
  // memory.
  int32 number_of_bytes =
      (message.height - 1) * message.pitch +
      image::ComputePitch(texture_object->format(), message.width);
  if (message.offset + number_of_bytes > info->size ||
      message.offset + number_of_bytes < message.offset) {
    O3D_ERROR(service_locator_)
        << "Offset + size as computed by width, height and pitch"
        << " exceeds allocated shared memory size ("
        << message.offset << " + " << number_of_bytes << " > "
        << info->size;
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  int mip_width =
      image::ComputeMipDimension(message.level, texture_object->width());
  int mip_height =
      image::ComputeMipDimension(message.level, texture_object->height());

  if (message.x < 0 || message.width < 0 ||
      message.y < 0 || message.height < 0 ||
      message.x + message.width > mip_width ||
      message.y + message.height > mip_height) {
    O3D_ERROR(service_locator_)
        << "rect out of range ("
        << message.x << ", " << message.y << ", " << message.width
        << message.height << ")";
    SendBooleanResponse(client->client_handle(), false);
    return false;
  }

  void *target_address =
      PointerFromVoidPointer<void*>(info->mapped_address, message.offset);
  texture_object->SetRect(
      message.level, message.x, message.y,
      message.width, message.height,
      target_address,
      message.pitch);

  SendBooleanResponse(client->client_handle(), true);
  has_new_texture_ = true;
  return true;
}

// Processes a request to register a client-allocated shared memory
// buffer on behalf of a connected client.  Parses the arguments of
// the message to determine how much space is being passed. It maps
// the shared memory buffer into the local address space and sends a
// message back to the client with the newly allocated shared memory
// ID.
bool MessageQueue::ProcessMessageRegisterSharedMemory(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageRegisterSharedMemory::Msg& message) {
  if (header->iov_length != 1 ||
      header->handle_count != 1) {
    LOG(ERROR) << "Malformed message for REGISTER_SHARED_MEMORY";
    return false;
  }

  int32 mem_size = message.mem_size;
  if (mem_size <= 0 ||
      mem_size > MessageRegisterSharedMemory::Msg::kMaxSharedMemSize) {
    LOG(ERROR) << "Invalid mem size sent: " << mem_size
               << "(max size = "
               << MessageRegisterSharedMemory::Msg::kMaxSharedMemSize << ")";
    return false;
  }

  // Fetch the handle to the preexisting shared memory object.
  nacl::Handle shared_memory = header->handles[0];
  if (shared_memory == nacl::kInvalidHandle) {
    LOG_IMC_ERROR("Invalid shared memory object registered");
    return false;
  }

  // Map it in local address space.
  void* shared_region = nacl::Map(0,
                                  mem_size,
                                  nacl::kProtRead | nacl::kProtWrite,
                                  nacl::kMapShared,
                                  shared_memory,
                                  0);
  if (shared_region == nacl::kMapFailed) {
    LOG_IMC_ERROR("Failed to map shared memory");
    nacl::Close(shared_memory);
    return false;
  }

  // Create a unique id for the shared memory buffer.
  int32 buffer_id = next_shared_memory_id_++;

  // Send the buffer id back to the client.
  nacl::MessageHeader response_header;
  nacl::IOVec id_vec;
  id_vec.base = &buffer_id;
  id_vec.length = sizeof(buffer_id);

  response_header.iov = &id_vec;
  response_header.iov_length = 1;
  response_header.handles = NULL;
  response_header.handle_count = 0;
  int result = nacl::SendDatagram(client->client_handle(), &response_header, 0);

  if (result != sizeof(buffer_id)) {
    LOG_IMC_ERROR("Failed to send shared memory ID back to the client");
    nacl::Unmap(shared_region, mem_size);
    nacl::Close(shared_memory);
    return false;
  }

  // Register the newly mapped shared memory with the connected client.
  client->RegisterSharedMemory(buffer_id,
                               shared_memory,
                               shared_region,
                               mem_size);

  return true;
}

// Processes a request to unregister a client-allocated shared memory
// buffer, referenced by ID.
bool MessageQueue::ProcessMessageUnregisterSharedMemory(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageUnregisterSharedMemory::Msg& message) {
  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for UNREGISTER_SHARED_MEMORY";
    return false;
  }

  bool res = client->UnregisterSharedMemory(message.buffer_id);
  SendBooleanResponse(client->client_handle(), res);
  return res;
}

// Processes a request to Render.
bool MessageQueue::ProcessMessageRender(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageRender::Msg& message) {
  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for RENDER";
    return false;
  }

  Renderer* renderer(service_locator_->GetService<Renderer>());
  if (renderer) {
    renderer->set_need_to_render(true);
  }
  return true;
}

// Processes a request to SetMaxFPS.
bool MessageQueue::ProcessMessageSetMaxFPS(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageSetMaxFPS::Msg& message) {
  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for SET_MAX_FPS";
    return false;
  }

  Renderer* renderer(service_locator_->GetService<Renderer>());
  if (renderer) {
    renderer->set_max_fps(message.max_fps);
  }

  SendBooleanResponse(client->client_handle(), true);
  return true;
}

// Processes a request to get the O3D version.
bool MessageQueue::ProcessMessageGetVersion(
    ConnectedClient* client,
    int message_length,
    nacl::MessageHeader* header,
    nacl::Handle* handles,
    const MessageGetVersion::Msg& message) {
  if (header->iov_length != 1 ||
      header->handle_count != 0) {
    LOG(ERROR) << "Malformed message for GET_VERSION";
    return false;
  }

  ClientInfoManager* manager(service_locator_->GetService<ClientInfoManager>());
  static const char* const kNullString = "";
  const char* version = kNullString;
  if (manager) {
    version = manager->client_info().version().c_str();
  }

  // Send the version string.
  DCHECK_LT(strlen(version), static_cast<size_t>(MAX_VERSION_STRING_LENGTH));
  MessageGetVersion::Response response(version);

  nacl::MessageHeader response_header;
  nacl::IOVec id_vec;
  id_vec.base = &response.data;
  id_vec.length = sizeof(response.data);

  response_header.iov = &id_vec;
  response_header.iov_length = 1;
  response_header.handles = NULL;
  response_header.handle_count = 0;
  nacl::SendDatagram(client->client_handle(), &response_header, 0);

  return true;
}



}  // namespace o3d
