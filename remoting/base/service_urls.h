// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SERVICE_URLS_H_
#define REMOTING_BASE_SERVICE_URLS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/singleton.h"

namespace remoting {

// This class contains the URLs to the services used by the host (except for
// Gaia, which has its own GaiaUrls class. In debug builds, it allows these URLs
// to be overriden by command line switches, allowing the host process to be
// pointed at alternate/test servers.
class ServiceUrls {
 public:
  static ServiceUrls* GetInstance();

  // Remoting directory REST API URLs.
  const std::string& directory_base_url() const;
  const std::string& directory_hosts_url() const;

  // XMPP Server configuration.
  const std::string& xmpp_server_address() const;
  bool xmpp_server_use_tls() const;

  // Remoting directory bot JID (for registering hosts, logging, heartbeats).
  const std::string& directory_bot_jid() const;

 private:
  friend struct DefaultSingletonTraits<ServiceUrls>;

  ServiceUrls();
  virtual ~ServiceUrls();

  std::string directory_base_url_;
  std::string directory_hosts_url_;
  std::string xmpp_server_address_;
  bool xmpp_server_use_tls_;
  std::string directory_bot_jid_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUrls);
};

}  // namespace remoting

#endif  // REMOTING_BASE_SERVICE_URLS_H_
