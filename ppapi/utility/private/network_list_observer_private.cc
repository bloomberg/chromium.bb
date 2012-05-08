// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/utility/private/network_list_observer_private.h"

#include "ppapi/cpp/private/network_list_private.h"
#include "ppapi/cpp/module.h"

namespace pp {

NetworkListObserverPrivate::NetworkListObserverPrivate(
    const InstanceHandle& instance)
    : PP_ALLOW_THIS_IN_INITIALIZER_LIST(
        monitor_(instance,
                 &NetworkListObserverPrivate::NetworkListCallbackHandler,
                 this)) {
}

NetworkListObserverPrivate::~NetworkListObserverPrivate() {
}

// static
void NetworkListObserverPrivate::NetworkListCallbackHandler(
    void* user_data,
    PP_Resource list_resource) {
  NetworkListObserverPrivate* object =
      static_cast<NetworkListObserverPrivate*>(user_data);
  NetworkListPrivate list(PASS_REF, list_resource);
  object->OnNetworkListChanged(list);
}

}  // namespace pp
