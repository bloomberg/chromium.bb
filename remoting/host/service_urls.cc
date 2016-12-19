// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/service_urls.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "google_apis/google_api_keys.h"
#include "remoting/signaling/remoting_bot.h"

// Configurable service data.
const char kDirectoryBaseUrl[] = "https://www.googleapis.com/chromoting/v1";
const char kGcdBaseUrl[] = "https://www.googleapis.com/clouddevices/v1";
const char kXmppServerAddress[] = "talk.google.com:443";
const char kXmppServerAddressForMe2MeHost[] = "talk.google.com:5222";
const bool kXmppServerUseTls = true;
const char kGcdJid[] = "clouddevices.gserviceaccount.com";
const char kNetworkTraversalApiUrlBase[] =
    "https://networktraversal.googleapis.com/v1alpha/iceconfig?key=";

// Command line switches.
#if !defined(NDEBUG)
const char kDirectoryBaseUrlSwitch[] = "directory-base-url";
const char kGcdBaseUrlSwitch[] = "gcd-base-url";
const char kXmppServerAddressSwitch[] = "xmpp-server-address";
const char kXmppServerDisableTlsSwitch[] = "disable-xmpp-server-tls";
const char kDirectoryBotJidSwitch[] = "directory-bot-jid";
const char kGcdJidSwitch[] = "gcd-jid";
const char kIceConfigUrl[] = "ice_config_url";
#endif  // !defined(NDEBUG)

// Non-configurable service paths.
const char kDirectoryHostsSuffix[] = "/@me/hosts/";

namespace remoting {

ServiceUrls::ServiceUrls()
    : directory_base_url_(kDirectoryBaseUrl),
      gcd_base_url_(kGcdBaseUrl),
      xmpp_server_address_(kXmppServerAddress),
      xmpp_server_address_for_me2me_host_(kXmppServerAddressForMe2MeHost),
      xmpp_server_use_tls_(kXmppServerUseTls),
      directory_bot_jid_(kRemotingBotJid),
      gcd_jid_(kGcdJid),
      ice_config_url_(kNetworkTraversalApiUrlBase +
                      google_apis::GetRemotingAPIKey()) {
#if !defined(NDEBUG)
  // Allow debug builds to override urls via command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  CHECK(command_line);
  if (command_line->HasSwitch(kDirectoryBaseUrlSwitch)) {
    directory_base_url_ = command_line->GetSwitchValueASCII(
        kDirectoryBaseUrlSwitch);
  }
  if (command_line->HasSwitch(kGcdBaseUrlSwitch)) {
    gcd_base_url_ = command_line->GetSwitchValueASCII(kGcdBaseUrlSwitch);
  }
  if (command_line->HasSwitch(kXmppServerAddressSwitch)) {
    xmpp_server_address_ = command_line->GetSwitchValueASCII(
        kXmppServerAddressSwitch);
    xmpp_server_address_for_me2me_host_ = xmpp_server_address_;
  }
  if (command_line->HasSwitch(kXmppServerDisableTlsSwitch)) {
    xmpp_server_use_tls_ = false;
  }
  if (command_line->HasSwitch(kDirectoryBotJidSwitch)) {
    directory_bot_jid_ = command_line->GetSwitchValueASCII(
        kDirectoryBotJidSwitch);
  }
  if (command_line->HasSwitch(kGcdJidSwitch)) {
    gcd_jid_ = command_line->GetSwitchValueASCII(kGcdJidSwitch);
  }
  if (command_line->HasSwitch(kIceConfigUrl)) {
    ice_config_url_ = command_line->GetSwitchValueASCII(kIceConfigUrl);
  }
#endif  // !defined(NDEBUG)

  directory_hosts_url_ = directory_base_url_ + kDirectoryHostsSuffix;
}

ServiceUrls::~ServiceUrls() {}

ServiceUrls* remoting::ServiceUrls::GetInstance() {
  return base::Singleton<ServiceUrls>::get();
}

}  // namespace remoting
