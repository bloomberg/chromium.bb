// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_PPAPI_HOST_H_
#define PPAPI_HOST_PPAPI_HOST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/ppapi_host_export.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {

namespace proxy {
class ResourceMessageCallParams;
class ResourceMessageReplyParams;
}

namespace host {

class HostFactory;
class InstanceMessageFilter;
class ResourceHost;

// The host provides routing and tracking for resource message calls that
// come from the plugin to the host (browser or renderer), and the
// corresponding replies.
class PPAPI_HOST_EXPORT PpapiHost : public IPC::Sender, public IPC::Listener {
 public:
  // The sender is the channel to the plugin for outgoing messages. The factory
  // will be used to receive resource creation messages from the plugin. Both
  // pointers are owned by the caller and must outlive this class.
  PpapiHost(IPC::Sender* sender,
            HostFactory* host_factory,
            const PpapiPermissions& perms);
  virtual ~PpapiHost();

  const PpapiPermissions& permissions() const { return permissions_; }

  // Sender implementation. Forwards to the sender_.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Sends the given reply message to the plugin.
  void SendReply(const proxy::ResourceMessageReplyParams& params,
                 const IPC::Message& msg);

  // Adds the given message filter to the host. The PpapiHost will take
  // ownership of the pointer.
  void AddInstanceMessageFilter(scoped_ptr<InstanceMessageFilter> filter);

 private:
  friend class InstanceMessageFilter;

  // Message handlers.
  void OnHostMsgResourceCall(const proxy::ResourceMessageCallParams& params,
                             const IPC::Message& nested_msg);
  void OnHostMsgResourceCreated(const proxy::ResourceMessageCallParams& param,
                                PP_Instance instance,
                                const IPC::Message& nested_msg);
  void OnHostMsgResourceDestroyed(PP_Resource resource);

  // Returns null if the resource doesn't exist.
  host::ResourceHost* GetResourceHost(PP_Resource resource);

  // Non-owning pointer.
  IPC::Sender* sender_;

  // Non-owning pointer.
  HostFactory* host_factory_;

  PpapiPermissions permissions_;

  // Filters for instance messages. Note that since we don't support deleting
  // these dynamically we don't need to worry about modifications during
  // iteration. If we add that capability, this should be replaced with an
  // ObserverList.
  ScopedVector<InstanceMessageFilter> instance_message_filters_;

  typedef std::map<PP_Resource, linked_ptr<ResourceHost> > ResourceMap;
  ResourceMap resources_;

  DISALLOW_COPY_AND_ASSIGN(PpapiHost);
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_PPAPIE_HOST_H_
