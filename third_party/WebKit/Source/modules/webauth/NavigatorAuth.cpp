// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/NavigatorAuth.h"

#include "core/frame/Navigator.h"
#include "modules/webauth/WebAuthentication.h"

namespace blink {

NavigatorAuth& NavigatorAuth::From(Navigator& navigator) {
  NavigatorAuth* supplement = static_cast<NavigatorAuth*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorAuth(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

WebAuthentication* NavigatorAuth::authentication(Navigator& navigator) {
  return NavigatorAuth::From(navigator).authentication();
}

WebAuthentication* NavigatorAuth::authentication() {
  return webauthentication_;
}

DEFINE_TRACE(NavigatorAuth) {
  visitor->Trace(webauthentication_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorAuth::NavigatorAuth(Navigator& navigator) {
  if (navigator.GetFrame())
    webauthentication_ = WebAuthentication::Create(*navigator.GetFrame());
}

const char* NavigatorAuth::SupplementName() {
  return "NavigatorAuth";
}

}  // namespace blink
