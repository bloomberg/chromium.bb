// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_ERROR_OBSERVER_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_ERROR_OBSERVER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "net/base/net_export.h"

namespace net {

// Interface for observing JavaScript error messages from PAC scripts.
class NET_EXPORT_PRIVATE ProxyResolverErrorObserver {
 public:
  ProxyResolverErrorObserver() {}
  virtual ~ProxyResolverErrorObserver() {}

  // Handler for when an error is encountered. |line_number| may be -1
  // if a line number is not applicable to this error. |error| is a message
  // describing the error.
  //
  // Note on threading: This may get called from a worker thread. If the
  // backing proxy resolver is ProxyResolverV8Tracing, then it will not
  // be called concurrently, however it will be called from a different
  // thread than the proxy resolver's origin thread.
  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverErrorObserver);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVER_ERROR_OBSERVER_H_
