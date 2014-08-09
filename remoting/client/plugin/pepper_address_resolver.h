// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_ADDRESS_RESOLVER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_ADDRESS_RESOLVER_H_

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "third_party/webrtc/base/asyncresolverinterface.h"

namespace remoting {

// rtc::AsyncResolverInterface implementation that uses Pepper to resolve
// addresses.
class PepperAddressResolver : public rtc::AsyncResolverInterface {
 public:
  explicit PepperAddressResolver(const pp::InstanceHandle& instance);
  virtual ~PepperAddressResolver();

  // rtc::AsyncResolverInterface.
  virtual void Start(const rtc::SocketAddress& addr) OVERRIDE;
  virtual bool GetResolvedAddress(int family,
                                  rtc::SocketAddress* addr) const OVERRIDE;
  virtual int GetError() const OVERRIDE;
  virtual void Destroy(bool wait) OVERRIDE;

 private:
  void OnResolved(int32_t result);

  pp::HostResolver resolver_;

  rtc::SocketAddress ipv4_address_;
  rtc::SocketAddress ipv6_address_;

  int error_;

  pp::CompletionCallbackFactory<PepperAddressResolver> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperAddressResolver);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_ADDRESS_RESOLVER_H_
