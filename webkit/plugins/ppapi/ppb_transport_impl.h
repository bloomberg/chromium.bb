// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_TRANSPORT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_TRANSPORT_IMPL_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/base/candidate.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/resource.h"

namespace talk_base {
class NetworkManager;
}  // namespace talk_base

namespace cricket {
class HttpPortAllocator;
class P2PTransportChannel;
class TransportChannel;
class TransportChannelImpl;
}  // namespace cricket

namespace webkit {
namespace ppapi {

class PPB_Transport_Impl : public Resource, public sigslot::has_slots<> {
 public:
  static const PPB_Transport_Dev* GetInterface();

  explicit PPB_Transport_Impl(PluginInstance* instance);
  virtual ~PPB_Transport_Impl();

  bool Init(const char* name, const char* proto);

  // Resource override.
  virtual PPB_Transport_Impl* AsPPB_Transport_Impl();

  bool IsWritable() const;
  int32_t Connect(PP_CompletionCallback cb);
  int32_t GetNextAddress(PP_Var* address, PP_CompletionCallback cb);
  int32_t ReceiveRemoteAddress(PP_Var address);
  int32_t Recv(void* data, uint32_t len, PP_CompletionCallback cb);
  int32_t Send(const void* data, uint32_t len, PP_CompletionCallback cb);
  int32_t Close();

 private:
  void OnRequestSignaling();
  void OnCandidateReady(cricket::TransportChannelImpl* channel,
                        const cricket::Candidate& candidate);
  void OnWriteableState(cricket::TransportChannel*);
  void OnReadPacket(cricket::TransportChannel*, const char*, size_t);

  bool Serialize(const cricket::Candidate& candidate, PP_Var* address);
  bool Deserialize(PP_Var address, cricket::Candidate* candidate);

  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<cricket::HttpPortAllocator> allocator_;
  scoped_ptr<cricket::P2PTransportChannel> channel_;
  std::list<cricket::Candidate> local_candidates_;

  scoped_refptr<TrackedCompletionCallback> connect_callback_;

  scoped_refptr<TrackedCompletionCallback> next_address_callback_;

  scoped_refptr<TrackedCompletionCallback> recv_callback_;
  void* recv_buffer_;
  uint32_t recv_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Transport_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_TRANSPORT_IMPL_H_
