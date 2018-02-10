// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/NavigatorCredentials.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/credentialmanager/CredentialsContainer.h"

namespace blink {

NavigatorCredentials::NavigatorCredentials(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorCredentials& NavigatorCredentials::From(Navigator& navigator) {
  NavigatorCredentials* supplement =
      Supplement<Navigator>::From<NavigatorCredentials>(navigator);
  if (!supplement) {
    supplement = new NavigatorCredentials(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorCredentials::kSupplementName[] = "NavigatorCredentials";

CredentialsContainer* NavigatorCredentials::credentials(Navigator& navigator) {
  return NavigatorCredentials::From(navigator).credentials();
}

CredentialsContainer* NavigatorCredentials::credentials() {
  if (!credentials_container_)
    credentials_container_ = CredentialsContainer::Create();
  return credentials_container_.Get();
}

void NavigatorCredentials::Trace(blink::Visitor* visitor) {
  visitor->Trace(credentials_container_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
