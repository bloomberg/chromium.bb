/*
 * Copyright 2008, Google Inc.
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


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_

#include <assert.h>
#include <string.h>
#include <time.h>

#include <functional>
#include <set>
#include <map>

#include "native_client/src/shared/imc/nacl_htp.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_handle.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/npruntime/nprpc.h"

namespace nacl {

// Manages NPObject proxies and stubs between two processes. The NPNavigator
// class and the NPModule class extends the NPBridge class to represent the
// NaCl module end of the connection to the browser plugin, and the plugin end
// of the connection to the NaCl module, respectively.
class NPBridge {
  // The size of the RPC stack in bytes.
  static const size_t kStackSize = (1024 - 64) * 1024;  // TBD
  // Each element pushed on the stack is aligned at kAlign byte alignment.
  static const size_t kAlign = 1 << 2;

  // Rounds up size to the next multiple of kAlign byte.
  static size_t Align(size_t size) {
    size += kAlign - 1;
    size &= ~(kAlign - 1);
    return size;
  }

  // NPP of this plugin module.
  NPP npp_;
  // The process ID of the remote peer.
  int peer_pid_;
  // The size of NPVariant in the remote peer.
  size_t peer_npvariant_size_;
  // The connected socket handle to the remote peer.
  HtpHandle channel_;
  // Time when NPBridge starts waiting for the response from the remote peer.
  time_t waiting_since_;

  // The map of NPObject to NPObjectStub
  std::map<NPObject*, NPObjectStub*> stub_map_;
  // The map of NPCapability to NPObjectProxy
  std::map<const NPCapability, NPObjectProxy*> proxy_map_;

  // The next RPC request tag number.
  int tag_;
  // The RPC stack heap.
  char heap_[kStackSize];
  // The current top pointer in the RPC stack.
  char* top_;

  // The array of handles to send to or receive from the remote peer.
  HtpHandle handles_[kHandleCountMax];
  // The number of valid handles in handles_.
  size_t handle_count_;

  // If the current browser is based on WebKit, this flag is set to true.
  // NPBridge checks this flag to address the NPAPI runtime behavior differences
  // between WebKit and Gecko.
  bool is_webkit_;

  // Initializes the RPC stack.
  void Init() {
    top_ = heap_;
  }

  // Allocates size bytes from the RPC stack. Alloc() returns NULL if the
  // request fails.
  void* Alloc(size_t size) {
    size = Align(size);
    if (size <= GetFreeSize()) {
      void* p = top_;
      top_ += size;
      return p;
    }
    return 0;
  }

  // Frees size bytes to the RPC stack.
  void Free(size_t size) {
    size = Align(size);
    if (size <= static_cast<size_t>(top_ - heap_)) {
      top_ -= size;
    }
  }

  // Returns the number of available bytes in the RPC stack.
  size_t GetFreeSize() {
    return heap_ + kStackSize - top_;
  }

  // Returns the current top pointer in the RPC stack.
  void* top() const {
    return top_;
  }

  // Sets the current top pointer in the RPC stack to top.
  void set_top(void* top) {
    if (heap_ <= top && top <= heap_ + kStackSize) {
      top_ = static_cast<char*>(top);
    }
  }

 public:
  // The maximum number of command line arguments passed from the plugin to the
  // NaCl module.
  static const int kMaxArg = 64;  // TBD

  explicit NPBridge(NPP npp);
  virtual ~NPBridge();

  NPP npp() const {
    return npp_;
  }

  int peer_pid() const {
    return peer_pid_;
  }
  void set_peer_pid(int pid) {
    peer_pid_ = pid;
  }

  bool is_webkit() const {
    return is_webkit_;
  }
  void set_is_webkit(bool webkit) {
    is_webkit_ = webkit;
  }

  size_t peer_npvariant_size() const {
    return peer_npvariant_size_;
  }
  void set_peer_npvariant_size(size_t size) {
    peer_npvariant_size_ = size;
  }

  // Gets the socket handle connected to the remote peer.
  nacl::HtpHandle channel() const {
    return channel_;
  }

  // Sets the socket handle connected to the remote peer.
  void set_channel(nacl::Handle channel) {
    channel_ = CreateImcDesc(channel);
  }

  // Returns the number of handles currently saved in this NPBridge object.
  size_t handle_count() const {
    return handle_count_;
  }

  // Clears the number of handles currently saved in this NPBridge object.
  void clear_handle_count() {
    handle_count_ = 0;
  }

  // Gets an handle at index.
  HtpHandle handle(size_t index) const {
    assert(index < handle_count_);
    return handles_[index];
  }

  // Adds an handle to send to the remote peer.
  void add_handle(HtpHandle handle) {
    if (handle_count_ < kHandleCountMax - 1) {
      handles_[handle_count_++] = handle;
    }
  }

  // Sends an RPC request to the remote peer, and waits for the response.
  // Request() returns the RPC response header received from the remote peer
  // and the size of the response message is stored in the buffer pointed by
  // the length parameter. Request() returns NULL on failure.
  // The 1st element in the iov array must be the pointer to the request
  // header, and the extra data can be sent using the iov array. The number of
  // elements in the iov array must be specified by the iov_length parameter.
  RpcHeader* Request(RpcHeader* request, IOVec* iov, size_t iov_length,
                     int* length);

  // Waits for the response from the remote peer for the request. Wait()
  // returns the RPC response header received from the remote peer
  // and the size of the response message is stored in the buffer pointed by
  // the length parameter. Wait() returns NULL on failure.
  // Internally, Wait() processes the requests received from the remote peer
  // while waiting for the response. If request is NULL, Wait() simply processes
  // the received requests.
  RpcHeader* Wait(const RpcHeader* request, int* length);

  // Dispatches the received request to the NPObject stub. The NPNavigator
  // class and the NPModule class override this function to process request
  // messages not handled by the NPBridge class.
  virtual int Dispatch(RpcHeader* request, int length);

  // Sends an RPC response to the remote peer. Respond() returns the number of
  // bytes sent on success, or -1 on failure.
  // The 1st element in the iov array must be the pointer to the response
  // header, and the extra data can be sent using the iov array. The number of
  // elements in the iov array must be specified by the iov_length parameter.
  int Respond(const RpcHeader* reply, IOVec* iov, size_t iov_length);

  // Returns the time in seconds since Wait() started waiting for the response
  // from the remote peer.
  time_t IsWaitingFor() {
    if (waiting_since_ == 0) {
      return 0;
    }
    return time(NULL) - waiting_since_;
  }

  // Creates an NPObject stub for the specified object. On success, CreateStub()
  // returns 1 and NPCapability for the stub is filled in the buffer pointed by
  // the cap parameter. CreateStub() returns 0 on failure.
  int CreateStub(NPObject* object, NPCapability* cap);

  // Returns the NPObject stub that corresponds to the specified object.
  // LookupStub() returns NULL if no correspoing stub is found.
  NPObjectStub* LookupStub(NPObject* object);


  // Returns the NPObject stub that corresponds to the specified capability.
  // GetStub() returns NULL if no correspoing stub is found.
  NPObjectStub* GetStub(const NPCapability& capability);

  // Removes the specified NPObject stub.
  void RemoveStub(NPObjectStub* stub);

  // Creates a proxy NPObject for the specified capability. CreateProxy()
  // returns pointer to the proxy NPObject on success, and NULL on failure.
  NPObject* CreateProxy(const NPCapability& capability);

  // Returns the NPObject proxy that corresponds to the specified capability.
  // LookupProxy() returns NULL if no correspoing stub is found.
  NPObjectProxy* LookupProxy(const NPCapability& capability);

  // Adds the specified proxy to the map of NPCapability to NPObjectProxy
  void AddProxy(NPObjectProxy* proxy);

  // Removes the specified proxy to the map of NPCapability to NPObjectProxy
  void RemoveProxy(NPObjectProxy* proxy);

  friend class RpcStack;
  friend class RpcArg;
};

// Represents the RPC stack frame for sending a request.
class RpcStack {
  // The pointer to the NPBridge for which this RPC stack frame is created.
  NPBridge* bridge_;
  // The base pointer of this RPC stack frame.
  void* base_;
  // Set to true if the RPC stack is overflowed.
  bool overflow_;

 public:
  // Creates a new RPC stack frame from the top of the RPC stack of the
  // specified NPBridge.
  explicit RpcStack(NPBridge* bridge)
      : bridge_(bridge),
        base_(bridge->top()),
        overflow_(false) {
  }

  // Releases this stack frame.
  ~RpcStack() {
    bridge_->set_top(base_);
  }

  // Allocates size bytes from the RPC stack. Alloc() returns NULL if the
  // request fails.
  void* Alloc(size_t size) {
    if (overflow_) {
      return NULL;
    }
    void* buffer = bridge_->Alloc(size);
    if (buffer == NULL) {
      overflow_ = true;
    }
    return buffer;
  }

  // Returns the current top pointer in the RPC stack.
  void* top() {
    return bridge_->top();
  }

  // Returns the number of available bytes in the RPC stack.
  size_t GetFreeSize() {
    return bridge_->GetFreeSize();
  }

  // Pushes only the optional data of the name on the RPC stack.
  void* Push(NPIdentifier name);

  // Pushes only the optional data of the variant on the RPC stack.
  void* Push(const NPVariant& variant, bool param);

  // Pushes only NPCapability for the object on the RPC stack.
  void* Push(NPObject* object);

  // If not overflowed, fills in vecp and returns the vecp incremented by one.
  // If overflowed, return vecp as it is.
  IOVec* SetIOVec(IOVec* vecp) {
    if (!overflow_) {
      vecp->base = base_;
      vecp->length = static_cast<char*>(top()) - static_cast<char*>(base_);
      return ++vecp;
    }
    return vecp;
  }
};

// Represents the RPC stack frame for a received request or a response.
// The RPC messages used in the NPAPI bridge have the following format:
//
//        +-----------+---- RPC Message ----------------+
//        | RpcHeader | base portion | optional portion |
//        +-----------^--------------^------------------+
//                    |              |
//                   base_          opt_
//
// Once the RPC stack frame is created, the base pointer and the option pointer
// must be set to the beginning of the base portion and the optional portion,
// respectively with Step() and StepOption() methods.
class RpcArg {
  // The pointer to the NPBridge for which this RPC stack frame is created.
  NPBridge* bridge_;
  // The current base pointer of this RPC stack frame.
  char* base_;
  // The current option pointer of this RPC stack frame.
  char* opt_;
  // The end of this RPC stack frame.
  char* limit_;
  // The number of handles retrieved by GetHandle().
  size_t handle_index_;

  // Moves ptr size bytes forward in the RPC stack frame.
  char* Step(char* ptr, size_t size) {
    if (ptr + size == limit_) {
      // The last item on the stack does not have to be aligned at the tail.
      ptr = limit_;
      return ptr;
    }
    size = NPBridge::Align(size);
    if (ptr + size <= limit_) {
      ptr += size;
      return ptr;
    }
    return NULL;
  }

  // Gets a handle received from the remote peer, and creates an NPHandleObject
  // for that handle. GetHandleObject() returns a pointer to an NPObject on
  // success, or NULL on failure.
  NPObject* GetHandleObject() {
    HtpHandle handle = GetHandle();
    NPObject* object = new(std::nothrow) NPHandleObject(bridge_, handle);
    if (object == NULL) {
      Close(handle);
    }
    return object;
  }

 public:
  // Creates a new RPC stack frame for an RPC request or a response received
  // from the specified bridge. The base parameter should point to the RPC
  // header, and the length parameter should be the size of the RPC message.
  RpcArg(NPBridge* bridge, void* base, int length)
      : bridge_(bridge),
        base_(static_cast<char*>(base)),
        opt_(base_),
        limit_(base_ + length),
        handle_index_(0) {
  }

  // Releases this RPC stack frame.
  ~RpcArg() {
  }

  // Moves the base pointer and the option pointer size bytes forward.
  void* Step(size_t size) {
    base_ = opt_ = Step(base_, size);
    return base_;
  }

  // Moves the option pointer size bytes forward.
  void* StepOption(size_t size) {
    opt_ = Step(opt_, size);
    return base_;
  }

  // Pops a character string from the RPC stack frame.
  const char* GetString();

  // Pops an NPIdentifier from the RPC stack frame.
  NPIdentifier GetIdentifier();

  // Pops an NPVariant from the RPC stack frame. The param parameter should be
  // true if this stack frame is for the received request message, and false
  // if this stack frame is for the received response message.
  const NPVariant* GetVariant(bool param);

  // Pops an NPObject from the RPC stack frame.
  NPObject* GetObject();

  // Pops an NPCapability from the RPC stack frame.
  NPCapability* GetCapability();

  // Pops an NPSize structure from the RPC stack frame.
  NPSize* GetSize();

  // Pops an NPRect structure from the RPC stack frame.
  NPRect* GetRect();

  // Returns the number of handles received from the remote peer.
  size_t GetHandleCount() const {
    assert(handle_index_ <= bridge_->handle_count());
    return bridge_->handle_count() - handle_index_;
  }

  // Gets a handle received from the remote peer. GetHandle() should not be
  // called more than GetHandleCount() times per message.
  HtpHandle GetHandle() {
    HtpHandle handle = bridge_->handle(handle_index_);
    if (handle_index_ < bridge_->handle_count()) {
      ++handle_index_;
    }
    return handle;
  }

  // Closes received handles not retrieved by GetHandle().
  void CloseUnusedHandles() {
    while (handle_index_ < bridge_->handle_count()) {
      HtpHandle unused_handle = bridge_->handle(handle_index_);
      Close(unused_handle);
      ++handle_index_;
    }
    bridge_->clear_handle_count();
  }
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_
