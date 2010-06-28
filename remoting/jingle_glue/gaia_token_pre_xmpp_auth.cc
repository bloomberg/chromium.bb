// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/gaia_token_pre_xmpp_auth.h"

#include <algorithm>

#include "talk/base/socketaddress.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/saslcookiemechanism.h"

namespace remoting {

namespace {
const char kGaiaAuthMechanism[] = "X-GOOGLE-TOKEN";
}  // namespace

GaiaTokenPreXmppAuth::GaiaTokenPreXmppAuth(
    const std::string& username,
    const std::string& token,
    const std::string& token_service)
    : username_(username),
      token_(token),
      token_service_(token_service) { }

GaiaTokenPreXmppAuth::~GaiaTokenPreXmppAuth() { }

void GaiaTokenPreXmppAuth::StartPreXmppAuth(
    const buzz::Jid& jid,
    const talk_base::SocketAddress& server,
    const talk_base::CryptString& pass,
    const std::string& auth_cookie) {
  SignalAuthDone();
}

bool GaiaTokenPreXmppAuth::IsAuthDone() const {
  return true;
}

bool GaiaTokenPreXmppAuth::IsAuthorized() const {
  return true;
}

bool GaiaTokenPreXmppAuth::HadError() const {
  return false;
}

int GaiaTokenPreXmppAuth::GetError() const {
  return 0;
}

buzz::CaptchaChallenge GaiaTokenPreXmppAuth::GetCaptchaChallenge() const {
  return buzz::CaptchaChallenge();
}

std::string GaiaTokenPreXmppAuth::GetAuthCookie() const {
  return std::string();
}

std::string GaiaTokenPreXmppAuth::ChooseBestSaslMechanism(
    const std::vector<std::string> & mechanisms, bool encrypted) {
  return (std::find(mechanisms.begin(),
                    mechanisms.end(), kGaiaAuthMechanism) !=
          mechanisms.end()) ? kGaiaAuthMechanism : "";
}

buzz::SaslMechanism* GaiaTokenPreXmppAuth::CreateSaslMechanism(
    const std::string& mechanism) {
  if (mechanism != kGaiaAuthMechanism)
    return NULL;
  return new buzz::SaslCookieMechanism(
      kGaiaAuthMechanism, username_, token_, token_service_);
}

}  // namespace remoting
