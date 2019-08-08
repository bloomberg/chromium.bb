// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/service_urls.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/remoting_bot.h"

// Configurable service data.
constexpr char kDirectoryBaseUrl[] = "https://www.googleapis.com/chromoting/v1";
constexpr char kGcdBaseUrl[] = "https://www.googleapis.com/clouddevices/v1";
constexpr char kXmppServerAddress[] = "talk.google.com:443";
constexpr char kXmppServerAddressForMe2MeHost[] = "talk.google.com:5222";
constexpr bool kXmppServerUseTls = true;
constexpr char kGcdJid[] = "clouddevices.gserviceaccount.com";
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";

// Command line switches.
#if !defined(NDEBUG)
constexpr char kDirectoryBaseUrlSwitch[] = "directory-base-url";
constexpr char kGcdBaseUrlSwitch[] = "gcd-base-url";
constexpr char kXmppServerAddressSwitch[] = "xmpp-server-address";
constexpr char kXmppServerDisableTlsSwitch[] = "disable-xmpp-server-tls";
constexpr char kDirectoryBotJidSwitch[] = "directory-bot-jid";
constexpr char kGcdJidSwitch[] = "gcd-jid";
constexpr char kFtlServerEndpointSwitch[] = "ftl-server-endpoint";
#endif  // !defined(NDEBUG)

// Non-configurable service paths.
constexpr char kDirectoryHostsSuffix[] = "/@me/hosts/";
constexpr char kDirectoryIceConfigSuffix[] = "/@me/iceconfig";

namespace remoting {

ServiceUrls::ServiceUrls()
    : directory_base_url_(kDirectoryBaseUrl),
      gcd_base_url_(kGcdBaseUrl),
      xmpp_server_address_(kXmppServerAddress),
      xmpp_server_address_for_me2me_host_(kXmppServerAddressForMe2MeHost),
      xmpp_server_use_tls_(kXmppServerUseTls),
      directory_bot_jid_(kRemotingBotJid),
      gcd_jid_(kGcdJid),
      ftl_server_endpoint_(kFtlServerEndpoint) {
#if !defined(NDEBUG)
  // The command line may not be initialized when running as a PNaCl plugin.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    // Allow debug builds to override urls via command line.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    CHECK(command_line);
    if (command_line->HasSwitch(kDirectoryBaseUrlSwitch)) {
      directory_base_url_ =
          command_line->GetSwitchValueASCII(kDirectoryBaseUrlSwitch);
    }
    if (command_line->HasSwitch(kGcdBaseUrlSwitch)) {
      gcd_base_url_ = command_line->GetSwitchValueASCII(kGcdBaseUrlSwitch);
    }
    if (command_line->HasSwitch(kXmppServerAddressSwitch)) {
      xmpp_server_address_ =
          command_line->GetSwitchValueASCII(kXmppServerAddressSwitch);
      xmpp_server_address_for_me2me_host_ = xmpp_server_address_;
    }
    if (command_line->HasSwitch(kXmppServerDisableTlsSwitch)) {
      xmpp_server_use_tls_ = false;
    }
    if (command_line->HasSwitch(kDirectoryBotJidSwitch)) {
      directory_bot_jid_ =
          command_line->GetSwitchValueASCII(kDirectoryBotJidSwitch);
    }
    if (command_line->HasSwitch(kGcdJidSwitch)) {
      gcd_jid_ = command_line->GetSwitchValueASCII(kGcdJidSwitch);
    }
    if (command_line->HasSwitch(kFtlServerEndpointSwitch)) {
      ftl_server_endpoint_ =
          command_line->GetSwitchValueASCII(kFtlServerEndpointSwitch);
    }
  }
#endif  // !defined(NDEBUG)

  directory_hosts_url_ = directory_base_url_ + kDirectoryHostsSuffix;
  ice_config_url_ = directory_base_url_ + kDirectoryIceConfigSuffix;
}

ServiceUrls::~ServiceUrls() = default;

ServiceUrls* remoting::ServiceUrls::GetInstance() {
  return base::Singleton<ServiceUrls>::get();
}

}  // namespace remoting
