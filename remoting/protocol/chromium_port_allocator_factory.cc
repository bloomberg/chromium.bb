// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_port_allocator_factory.h"

#include "base/logging.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/network_settings.h"

namespace remoting {
namespace protocol {

ChromiumPortAllocatorFactory::ChromiumPortAllocatorFactory(
    const NetworkSettings& network_settings,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : network_settings_(network_settings),
      url_request_context_getter_(url_request_context_getter) {}

ChromiumPortAllocatorFactory::~ChromiumPortAllocatorFactory() {}

scoped_ptr<PortAllocatorFactory> ChromiumPortAllocatorFactory::Create(
    const NetworkSettings& network_settings,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  return scoped_ptr<PortAllocatorFactory>(new ChromiumPortAllocatorFactory(
      network_settings, url_request_context_getter));
}

cricket::PortAllocator* ChromiumPortAllocatorFactory::CreatePortAllocator() {
  scoped_ptr<ChromiumPortAllocator> port_allocator(
      ChromiumPortAllocator::Create(url_request_context_getter_,
                                    network_settings_));
  return port_allocator.release();
}

}  // namespace protocol
}  // namespace remoting

