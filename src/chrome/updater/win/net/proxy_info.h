// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_PROXY_INFO_H_
#define CHROME_UPDATER_WIN_NET_PROXY_INFO_H_

#include "base/strings/string16.h"

namespace updater {

struct ProxyInfo {
  ProxyInfo();
  ProxyInfo(bool auto_detect,
            const base::string16& auto_config_url,
            const base::string16& proxy,
            const base::string16& proxy_bypass);
  ~ProxyInfo();

  ProxyInfo(const ProxyInfo& proxy_info);
  ProxyInfo& operator=(const ProxyInfo& proxy_info);

  ProxyInfo(ProxyInfo&& proxy_info);
  ProxyInfo& operator=(ProxyInfo&& proxy_info);

  // Specifies the configuration is Web Proxy Auto Discovery (WPAD).
  bool auto_detect = false;

  // The url of the proxy auto configuration (PAC) script, if known.
  base::string16 auto_config_url;

  // Named proxy information.
  // The proxy string is usually something as "http=foo:80;https=bar:8080".
  // According to the documentation for WINHTTP_PROXY_INFO, multiple proxies
  // are separated by semicolons or whitespace.
  base::string16 proxy;
  base::string16 proxy_bypass;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_PROXY_INFO_H_
