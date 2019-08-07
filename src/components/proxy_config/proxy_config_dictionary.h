// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_DICTIONARY_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_DICTIONARY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "components/proxy_config/proxy_config_export.h"
#include "components/proxy_config/proxy_prefs.h"

namespace net {
class ProxyServer;
}

// Factory and wrapper for proxy config dictionaries that are stored
// in the user preferences. The dictionary has the following structure:
// {
//   mode: string,
//   server: string,
//   pac_url: string,
//   bypass_list: string
// }
// See proxy_config_dictionary.cc for the structure of the respective strings.
class PROXY_CONFIG_EXPORT ProxyConfigDictionary {
 public:
  // Takes ownership of |dict| (|dict| will be moved to |dict_|).
  explicit ProxyConfigDictionary(base::Value dict);
  ~ProxyConfigDictionary();

  bool GetMode(ProxyPrefs::ProxyMode* out) const;
  bool GetPacUrl(std::string* out) const;
  bool GetPacMandatory(bool* out) const;
  bool GetProxyServer(std::string* out) const;
  bool GetBypassList(std::string* out) const;
  bool HasBypassList() const;

  const base::Value& GetDictionary() const;

  static base::Value CreateDirect();
  static base::Value CreateAutoDetect();
  static base::Value CreatePacScript(const std::string& pac_url,
                                     bool pac_mandatory);
  static base::Value CreateFixedServers(const std::string& proxy_server,
                                        const std::string& bypass_list);
  static base::Value CreateSystem();

  // Encodes the proxy server as "<url-scheme>=<proxy-scheme>://<proxy>".
  // Used to generate the |proxy_server| arg for CreateFixedServers().
  static void EncodeAndAppendProxyServer(const std::string& url_scheme,
                                         const net::ProxyServer& server,
                                         std::string* spec);

 private:
  bool GetString(const char* key, std::string* out) const;

  static base::Value CreateDictionary(ProxyPrefs::ProxyMode mode,
                                      const std::string& pac_url,
                                      bool pac_mandatory,
                                      const std::string& proxy_server,
                                      const std::string& bypass_list);

  base::Value dict_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigDictionary);
};

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_DICTIONARY_H_
