// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RESOURCE_MESSAGE_PARAMS_H_
#define PPAPI_PROXY_RESOURCE_MESSAGE_PARAMS_H_

#include <vector>

#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/serialized_structs.h"

namespace ppapi {
namespace proxy {

// Common parameters for resource call and reply params structures below.
class PPAPI_PROXY_EXPORT ResourceMessageParams {
 public:
  virtual ~ResourceMessageParams();

  PP_Resource pp_resource() const { return pp_resource_; }
  int32_t sequence() const { return sequence_; }

  const std::vector<SerializedHandle> handles() const { return handles_; }

  // Returns a pointer to the handle at the given index if it exists and is of
  // the given type. If the index doesn't exist or the handle isn't of the
  // given type, returns NULL. Note that the pointer will be into an internal
  // vector so will be invalidated if the params are mutated.
  const SerializedHandle* GetHandleOfTypeAtIndex(
      size_t index,
      SerializedHandle::Type type) const;

  // Helper functions to return shared memory handles passed in the params
  // struct. If the index has a valid handle of the given type, it will be
  // placed in the output parameter and the function will return true. If the
  // handle doesn't exist or is a different type, the functions will return
  // false and the output parameter will be untouched.
  //
  // Note that the handle could still be a "null" or invalid handle of
  // the right type and the functions will succeed.
  bool GetSharedMemoryHandleAtIndex(size_t index,
                                    base::SharedMemoryHandle* handle) const;
  bool GetSocketHandleAtIndex(size_t index,
                              IPC::PlatformFileForTransit* handle) const;

  // Appends the given handle to the list of handles sent with the call or
  // reply.
  void AppendHandle(const SerializedHandle& handle);

 protected:
  ResourceMessageParams();
  ResourceMessageParams(PP_Resource resource, int32_t sequence);

  virtual void Serialize(IPC::Message* msg) const;
  virtual bool Deserialize(const IPC::Message* msg, PickleIterator* iter);

 private:
  PP_Resource pp_resource_;

  // Identifier for this message. Sequence numbers are quasi-unique within a
  // resource, but will overlap between different resource objects.
  //
  // If you send a lot of messages, the ID may wrap around. This is OK. All IDs
  // are valid and 0 and -1 aren't special, so those cases won't confuse us.
  // In practice, if you send more than 4 billion messages for a resource, the
  // old ones will be long gone and there will be no collisions.
  //
  // If there is a malicious plugin (or exceptionally bad luck) that causes a
  // wraparound and collision the worst that will happen is that we can get
  // confused between different callbacks. But since these can only cause
  // confusion within the plugin and within callbacks on the same resource,
  // there shouldn't be a security problem.
  int32_t sequence_;

  // A list of all handles transferred in the message. Handles go here so that
  // the NaCl adapter can extract them generally when it rewrites them to
  // go between Windows and NaCl (Posix) apps.
  std::vector<SerializedHandle> handles_;
};

// Parameters common to all ResourceMessage "Call" requests.
class PPAPI_PROXY_EXPORT ResourceMessageCallParams
    : public ResourceMessageParams {
 public:
  ResourceMessageCallParams();
  ResourceMessageCallParams(PP_Resource resource, int32_t sequence);
  virtual ~ResourceMessageCallParams();

  void set_has_callback() { has_callback_ = true; }
  bool has_callback() const { return has_callback_; }

  virtual void Serialize(IPC::Message* msg) const OVERRIDE;
  virtual bool Deserialize(const IPC::Message* msg,
                           PickleIterator* iter) OVERRIDE;

 private:
  bool has_callback_;
};

// Parameters common to all ResourceMessage "Reply" requests.
class PPAPI_PROXY_EXPORT ResourceMessageReplyParams
    : public ResourceMessageParams {
 public:
  ResourceMessageReplyParams();
  ResourceMessageReplyParams(PP_Resource resource, int32_t sequence);
  virtual ~ResourceMessageReplyParams();

  void set_result(int32_t r) { result_ = r; }
  int32_t result() const { return result_; }

  virtual void Serialize(IPC::Message* msg) const OVERRIDE;
  virtual bool Deserialize(const IPC::Message* msg,
                           PickleIterator* iter) OVERRIDE;

 private:
  // Pepper "result code" for the callback.
  int32_t result_;
};

}  // namespace proxy
}  // namespace ppapi

namespace IPC {

template <> struct PPAPI_PROXY_EXPORT
ParamTraits<ppapi::proxy::ResourceMessageCallParams> {
  typedef ppapi::proxy::ResourceMessageCallParams param_type;
  static void Write(Message* m, const param_type& p) {
    p.Serialize(m);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

template <> struct PPAPI_PROXY_EXPORT
ParamTraits<ppapi::proxy::ResourceMessageReplyParams> {
  typedef ppapi::proxy::ResourceMessageReplyParams param_type;
  static void Write(Message* m, const param_type& p) {
    p.Serialize(m);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

}  // namespace IPC

#endif  // PPAPI_PROXY_RESOURCE_MESSAGE_PARAMS_H_
