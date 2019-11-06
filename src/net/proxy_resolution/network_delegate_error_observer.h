// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_NETWORK_DELEGATE_ERROR_OBSERVER_H_
#define NET_PROXY_RESOLUTION_NETWORK_DELEGATE_ERROR_OBSERVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_error_observer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {

class NetworkDelegate;

// An implementation of ProxyResolverErrorObserver that forwards PAC script
// errors to a NetworkDelegate object on the thread it lives on.
class NET_EXPORT_PRIVATE NetworkDelegateErrorObserver
    : public ProxyResolverErrorObserver {
 public:
  NetworkDelegateErrorObserver(NetworkDelegate* network_delegate,
                               base::SingleThreadTaskRunner* origin_runner);
  ~NetworkDelegateErrorObserver() override;

  static std::unique_ptr<ProxyResolverErrorObserver> Create(
      NetworkDelegate* network_delegate,
      const scoped_refptr<base::SingleThreadTaskRunner>& origin_runner);

  // ProxyResolverErrorObserver implementation.
  void OnPACScriptError(int line_number, const base::string16& error) override;

 private:
  class Core;

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDelegateErrorObserver);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_NETWORK_DELEGATE_ERROR_OBSERVER_H_
