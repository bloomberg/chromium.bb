// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/NavigatorAuth.h"

#include "core/frame/Navigator.h"
#include "modules/webauth/WebAuthentication.h"

namespace blink {

NavigatorAuth& NavigatorAuth::from(Navigator& navigator) {
  NavigatorAuth* supplement = static_cast<NavigatorAuth*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorAuth(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

WebAuthentication* NavigatorAuth::authentication(Navigator& navigator) {
  return NavigatorAuth::from(navigator).authentication();
}

WebAuthentication* NavigatorAuth::authentication() {
  return m_webauthentication;
}

DEFINE_TRACE(NavigatorAuth) {
  visitor->trace(m_webauthentication);
  Supplement<Navigator>::trace(visitor);
}

NavigatorAuth::NavigatorAuth(Navigator& navigator) {
  if (navigator.frame())
    m_webauthentication = WebAuthentication::create(*navigator.frame());
}

const char* NavigatorAuth::supplementName() {
  return "NavigatorAuth";
}

}  // namespace blink
