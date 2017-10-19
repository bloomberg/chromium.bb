// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/netinfo/NavigatorNetworkInformation.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/netinfo/NetworkInformation.h"

namespace blink {

NavigatorNetworkInformation::NavigatorNetworkInformation(Navigator& navigator)
    : ContextClient(navigator.GetFrame()) {}

NavigatorNetworkInformation& NavigatorNetworkInformation::From(
    Navigator& navigator) {
  NavigatorNetworkInformation* supplement =
      ToNavigatorNetworkInformation(navigator);
  if (!supplement) {
    supplement = new NavigatorNetworkInformation(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

NavigatorNetworkInformation*
NavigatorNetworkInformation::ToNavigatorNetworkInformation(
    Navigator& navigator) {
  return static_cast<NavigatorNetworkInformation*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
}

const char* NavigatorNetworkInformation::SupplementName() {
  return "NavigatorNetworkInformation";
}

NetworkInformation* NavigatorNetworkInformation::connection(
    Navigator& navigator) {
  return NavigatorNetworkInformation::From(navigator).connection();
}

NetworkInformation* NavigatorNetworkInformation::connection() {
  if (!connection_ && GetFrame()) {
    DCHECK(GetFrame()->DomWindow());
    connection_ = NetworkInformation::Create(
        GetFrame()->DomWindow()->GetExecutionContext());
  }
  return connection_.Get();
}

void NavigatorNetworkInformation::Trace(blink::Visitor* visitor) {
  visitor->Trace(connection_);
  Supplement<Navigator>::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
